// WT32-ETH01: WiFiManager + strona WWW ze stanem Ethernetu,
// zmianą adresu MAC Ethernet i nazwy urządzenia (zapis w NVS).

#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_mac.h>
#include <esp_eth.h>
#include <esp_netif.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include "webpage.h"

// --- Sprzęt WT32-ETH01 (LAN8720) ---
#define ETH_ADDR        1
#define ETH_POWER_PIN   16
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE_   ETH_CLOCK_GPIO0_IN

// --- Dioda WS2811 ---
#define LED_PIN         4
#define LED_COUNT       1
#define LED_TYPE        (NEO_GRB + NEO_KHZ800)   // jeśli kolory są zamienione: NEO_RGB; dla wolnych WS2811: NEO_KHZ400
#define LED_BRIGHTNESS  80                       // maksymalna jasność 0-255
#define LED_PULSE_MS    2000                     // okres pulsowania

// --- Czas / harmonogram ---
static const char *TZ_INFO     = "CET-1CEST,M3.5.0,M10.5.0/3";   // Polska (CET/CEST)
static const char *NTP_SERVER1 = "pool.ntp.org";
static const char *NTP_SERVER2 = "time.google.com";
static const uint16_t DEFAULT_ETH_ON_MIN  = 8 * 60;    // 08:00
static const uint16_t DEFAULT_ETH_OFF_MIN = 16 * 60;   // 16:00

// --- Konfiguracja ---
#define FW_VERSION "1.2.5"                          // zwiększaj przy każdej zmianie firmware
static const char *WEB_PASSWORD     = "12345678";   // hasło do strony (kokpitu)
static const char *DEFAULT_HOSTNAME = "wt32-eth01";
static const char *PORTAL_PASSWORD  = "12345678";   // hasło AP konfiguracji WiFi (min. 8 znaków)
static const uint16_t PORTAL_TIMEOUT_S = 180;       // po tym czasie portal się zamyka

Preferences prefs;
WiFiManager wm;
WebServer server(80);
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, LED_TYPE);

String hostname;
uint8_t savedMac[6];          // MAC Ethernet zapisany przez użytkownika (lub fabryczny)
uint8_t defaultMac[6];        // fabryczny MAC Ethernet (eFuse base + 3)
bool customMac = false;

bool schedEnabled = true;
uint16_t ethOnMin = DEFAULT_ETH_ON_MIN;     // minuty od północy
uint16_t ethOffMin = DEFAULT_ETH_OFF_MIN;
bool ethEnabled = true;                     // stan Ethernetu sterowany harmonogramem

volatile bool ethLink = false;
volatile bool ethGotIP = false;
String sessionId;            // token sesji zapisywany w ciasteczku po zalogowaniu
bool otaAuthorized = false;
bool serverStarted = false;
bool mdnsStarted = false;
uint32_t restartAt = 0;

// ---------------------------------------------------------------- MAC utils

static String macToString(const uint8_t *m) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(buf);
}

static bool parseMac(const String &s, uint8_t *out) {
    String h;
    for (char c : s) {
        if (isxdigit((unsigned char)c)) h += c;
        else if (c != ':' && c != '-' && c != ' ') return false;
    }
    if (h.length() != 12) return false;
    for (int i = 0; i < 6; i++) out[i] = strtoul(h.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    return true;
}

// Dodaje (lub odejmuje) wartość do 48-bitowego adresu MAC.
static void macAdd(const uint8_t *in, int64_t delta, uint8_t *out) {
    uint64_t v = 0;
    for (int i = 0; i < 6; i++) v = (v << 8) | in[i];
    v = (v + delta) & 0xFFFFFFFFFFFFULL;
    for (int i = 5; i >= 0; i--) { out[i] = v & 0xFF; v >>= 8; }
}

// W rdzeniu Arduino 2.x MAC Ethernetu = bazowy MAC + 3 (4 uniwersalne adresy).
// Żeby uzyskać wybrany MAC na Ethernecie ustawiamy bazowy MAC = wybrany - 3.
// Musi się to stać przed inicjalizacją WiFi i ETH.
static bool applyEthMac(const uint8_t *ethMac) {
    uint8_t base[6];
    macAdd(ethMac, -3, base);
    if (base[0] & 0x01) return false;   // bazowy MAC nie może być multicast
    return esp_base_mac_addr_set(base) == ESP_OK;
}

static bool validHostname(const String &h) {
    if (h.length() < 1 || h.length() > 32) return false;
    if (h[0] == '-' || h[h.length() - 1] == '-') return false;
    for (char c : h)
        if (!isalnum((unsigned char)c) && c != '-') return false;
    return true;
}

// ---------------------------------------------------------------- Ustawienia

static void loadSettings() {
    uint8_t efuse[6];
    esp_efuse_mac_get_default(efuse);
    macAdd(efuse, 3, defaultMac);

    prefs.begin("cfg", true);
    hostname = prefs.getString("hostname", DEFAULT_HOSTNAME);
    customMac = prefs.getBytes("ethmac", savedMac, 6) == 6;
    schedEnabled = prefs.getBool("sch_en", true);
    ethOnMin = prefs.getUShort("sch_on", DEFAULT_ETH_ON_MIN) % 1440;
    ethOffMin = prefs.getUShort("sch_off", DEFAULT_ETH_OFF_MIN) % 1440;
    sessionId = prefs.getString("sid", "");
    prefs.end();

    // Losowy token sesji, zapamiętany w NVS – logowanie przetrwa restart urządzenia.
    if (sessionId.length() != 32) {
        char buf[33];
        for (int i = 0; i < 4; i++) snprintf(buf + i * 8, 9, "%08x", esp_random());
        sessionId = buf;
        prefs.begin("cfg", false);
        prefs.putString("sid", sessionId);
        prefs.end();
    }

    if (!validHostname(hostname)) hostname = DEFAULT_HOSTNAME;
    if (!customMac) memcpy(savedMac, defaultMac, 6);
}

// ---------------------------------------------------------------- Czas i harmonogram

// Czas z NTP; po synchronizacji zegar ESP32 liczy dalej sam (także przy wyłączonym Ethernecie).
static bool timeValid() {
    return time(nullptr) > 1700000000;   // > 2023-11 => czas został zsynchronizowany
}

static int nowMinutes() {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_hour * 60 + tm.tm_min;
}

// Czy teraz jest w przedziale [włącz, wyłącz). Obsługuje przedział przez północ (np. 22:00-06:00).
static bool inSchedule() {
    if (!schedEnabled || ethOnMin == ethOffMin) return true;
    int now = nowMinutes();
    if (ethOnMin < ethOffMin) return now >= ethOnMin && now < ethOffMin;
    return now >= ethOnMin || now < ethOffMin;
}

static String minToStr(uint16_t m) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02u:%02u", m / 60, m % 60);
    return String(buf);
}

static bool parseHHMM(const String &s, uint16_t &out) {
    int h, m;
    if (sscanf(s.c_str(), "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return false;
    out = h * 60 + m;
    return true;
}

// Uchwyt sterownika Ethernet (klasa ETH w rdzeniu 2.x go nie udostępnia).
static esp_eth_handle_t ethHandle() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
    return netif ? (esp_eth_handle_t)esp_netif_get_io_driver(netif) : nullptr;
}

// Zapis rejestru PHY. W IDF 4.4 struktura sterownika zaczyna się od mediatora,
// więc uchwyt można rzutować na esp_eth_mediator_t (zależne od wersji rdzenia!).
static void phyWrite(uint32_t reg, uint32_t val) {
    esp_eth_mediator_t *med = (esp_eth_mediator_t *)ethHandle();
    if (med) med->phy_reg_write(med, ETH_ADDR, reg, val);
}

// Włączenie/wyłączenie Ethernetu: zatrzymanie sterownika + uśpienie PHY (port na switchu gaśnie).
static void setEthEnabled(bool en) {
    esp_eth_handle_t h = ethHandle();
    if (!h || en == ethEnabled) return;
    if (en) {
        phyWrite(0, 0x1200);   // BMCR: autonegocjacja + restart, wyjście z power-down
        esp_eth_start(h);
        Serial.println("[SCH] Ethernet WŁĄCZONY");
    } else {
        esp_eth_stop(h);
        phyWrite(0, 0x0800);   // BMCR: power-down
        ethLink = false;
        ethGotIP = false;
        Serial.println("[SCH] Ethernet WYŁĄCZONY");
    }
    ethEnabled = en;
}

static void scheduleTask() {
    static uint32_t last = 0;
    if (millis() - last < 1000) return;
    last = millis();
    // Bez znanego czasu Ethernet zostaje włączony (potrzebny m.in. do pobrania czasu z NTP).
    setEthEnabled(!timeValid() || inSchedule());
}

// ---------------------------------------------------------------- Zdarzenia sieci

static void onNetEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            ETH.setHostname(hostname.c_str());
            Serial.println("[ETH] start");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            ethLink = true;
            Serial.println("[ETH] kabel podłączony");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            ethGotIP = true;
            Serial.printf("[ETH] IP: %s, MAC: %s, %d Mb/s %s\n", ETH.localIP().toString().c_str(),
                          ETH.macAddress().c_str(), ETH.linkSpeed(), ETH.fullDuplex() ? "FD" : "HD");
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            ethLink = false;
            ethGotIP = false;
            Serial.println("[ETH] kabel odłączony");
            break;
        case ARDUINO_EVENT_ETH_STOP:
            ethLink = false;
            ethGotIP = false;
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------- WWW

static bool isAuthed() {
    return server.header("Cookie").indexOf("sid=" + sessionId) >= 0;
}

// Dla API: 401, gdy brak zalogowania (strona przeładuje się do ekranu logowania).
static bool requireAuth() {
    if (isAuthed()) return true;
    server.send(401, "text/plain", "Wymagane logowanie.");
    return false;
}

static void handleRoot() {
    if (isAuthed()) server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    else server.send_P(200, "text/html; charset=utf-8", LOGIN_HTML);
}

static void handleLogin() {
    if (server.arg("password") == WEB_PASSWORD) {
        server.sendHeader("Set-Cookie", "sid=" + sessionId + "; Path=/; Max-Age=2592000; HttpOnly; SameSite=Strict");
        server.sendHeader("Location", "/");
    } else {
        server.sendHeader("Location", "/?err=1");
    }
    server.send(303);
}

static void handleLogout() {
    server.sendHeader("Set-Cookie", "sid=; Path=/; Max-Age=0");
    server.sendHeader("Location", "/");
    server.send(303);
}

static void handleStatus() {
    if (!requireAuth()) return;
    bool wifiOk = WiFi.status() == WL_CONNECTED;
    String ethIp = ethGotIP ? ETH.localIP().toString() : "";
    String wifiIp = wifiOk ? WiFi.localIP().toString() : "";
    String ssid = wifiOk ? WiFi.SSID() : "";
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");

    char timeStr[24] = "";
    if (timeValid()) {
        time_t t = time(nullptr);
        struct tm tm;
        localtime_r(&t, &tm);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);
    }

    char json[960];
    snprintf(json, sizeof(json),
             "{\"hostname\":\"%s\",\"fw\":\"" FW_VERSION "\",\"build\":\"" __DATE__ " " __TIME__ "\",\"uptime\":%lu,\"time\":\"%s\",\"time_valid\":%s,"
             "\"sched\":{\"enabled\":%s,\"on\":\"%s\",\"off\":\"%s\",\"in_range\":%s},"
             "\"eth\":{\"enabled\":%s,\"link\":%s,\"ip\":\"%s\",\"speed\":%d,\"duplex\":%s,"
             "\"mac\":\"%s\",\"mac_saved\":\"%s\",\"mac_default\":\"%s\"},"
             "\"wifi\":{\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d}}",
             hostname.c_str(), (unsigned long)(millis() / 1000), timeStr, timeValid() ? "true" : "false",
             schedEnabled ? "true" : "false", minToStr(ethOnMin).c_str(), minToStr(ethOffMin).c_str(),
             (timeValid() && inSchedule()) ? "true" : "false",
             ethEnabled ? "true" : "false", (ethLink && ETH.linkUp()) ? "true" : "false", ethIp.c_str(),
             ethLink ? ETH.linkSpeed() : 0, (ethLink && ETH.fullDuplex()) ? "true" : "false",
             ETH.macAddress().c_str(), macToString(savedMac).c_str(), macToString(defaultMac).c_str(),
             wifiOk ? "true" : "false", ssid.c_str(), wifiIp.c_str(), wifiOk ? WiFi.RSSI() : 0);
    server.send(200, "application/json", json);
}

static void handleConfig() {
    if (!requireAuth()) return;
    String newHost = server.arg("hostname");
    newHost.trim();
    uint8_t newMac[6];

    if (!validHostname(newHost)) {
        server.send(400, "text/plain", "Błędna nazwa: dozwolone litery, cyfry i '-', max 32 znaki.");
        return;
    }
    if (!parseMac(server.arg("mac"), newMac)) {
        server.send(400, "text/plain", "Błędny format MAC (np. AA:BB:CC:DD:EE:FF).");
        return;
    }
    uint8_t base[6];
    macAdd(newMac, -3, base);
    if ((newMac[0] & 0x01) || (base[0] & 0x01)) {
        server.send(400, "text/plain", "MAC musi być adresem unicast (pierwszy bajt parzysty).");
        return;
    }

    prefs.begin("cfg", false);
    prefs.putString("hostname", newHost);
    if (memcmp(newMac, defaultMac, 6) == 0) prefs.remove("ethmac");
    else prefs.putBytes("ethmac", newMac, 6);
    prefs.end();

    Serial.printf("[CFG] zapisano: hostname=%s, MAC=%s\n", newHost.c_str(), macToString(newMac).c_str());
    server.send(200, "text/plain", "Zapisano. Restart urządzenia...");
    restartAt = millis() + 1000;
}

static void handleSchedule() {
    if (!requireAuth()) return;
    uint16_t on, off;
    if (!parseHHMM(server.arg("on"), on) || !parseHHMM(server.arg("off"), off)) {
        server.send(400, "text/plain", "Błędny format godziny (HH:MM).");
        return;
    }
    schedEnabled = server.arg("enabled") == "1";
    ethOnMin = on;
    ethOffMin = off;

    prefs.begin("cfg", false);
    prefs.putBool("sch_en", schedEnabled);
    prefs.putUShort("sch_on", ethOnMin);
    prefs.putUShort("sch_off", ethOffMin);
    prefs.end();

    Serial.printf("[SCH] harmonogram %s: %s - %s\n", schedEnabled ? "włączony" : "wyłączony",
                  minToStr(ethOnMin).c_str(), minToStr(ethOffMin).c_str());
    server.send(200, "text/plain", "Harmonogram zapisany.");
}

// Aktualizacja OTA: plik .bin wysyłany ze strony (multipart) zapisywany do drugiej partycji aplikacji.
static void handleUpdateUpload() {
    HTTPUpload &up = server.upload();
    switch (up.status) {
        case UPLOAD_FILE_START:
            otaAuthorized = isAuthed();
            if (!otaAuthorized) break;
            Serial.printf("[OTA] start: %s\n", up.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
            break;
        case UPLOAD_FILE_WRITE:
            if (otaAuthorized && Update.isRunning() && Update.write(up.buf, up.currentSize) != up.currentSize)
                Update.printError(Serial);
            break;
        case UPLOAD_FILE_END:
            if (!otaAuthorized) break;
            if (Update.end(true)) Serial.printf("[OTA] OK, %u bajtów\n", up.totalSize);
            else Update.printError(Serial);
            break;
        case UPLOAD_FILE_ABORTED:
            Update.abort();
            Serial.println("[OTA] przerwano");
            break;
    }
}

static void handleUpdateDone() {
    if (!requireAuth()) return;
    if (Update.hasError() || !Update.isFinished()) {
        server.send(500, "text/plain", String("Błąd aktualizacji: ") + Update.errorString());
        return;
    }
    server.send(200, "text/plain", "Firmware wgrany. Restart urządzenia...");
    restartAt = millis() + 1000;
}

static void handleReboot() {
    if (!requireAuth()) return;
    server.send(200, "text/plain", "Restart urządzenia...");
    restartAt = millis() + 1000;
}

static void handleWifiReset() {
    if (!requireAuth()) return;
    server.send(200, "text/plain", "Sieć WiFi skasowana. Połącz się z AP \"" + hostname + "-setup\" (hasło: " +
                                       PORTAL_PASSWORD + ").");
    wm.resetSettings();
    restartAt = millis() + 1500;
}

static void startServer() {
    const char *headers[] = {"Cookie"};
    server.collectHeaders(headers, 1);
    server.on("/", HTTP_GET, handleRoot);
    server.on("/login", HTTP_POST, handleLogin);
    server.on("/logout", HTTP_GET, handleLogout);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/config", HTTP_POST, handleConfig);
    server.on("/api/schedule", HTTP_POST, handleSchedule);
    server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
    server.on("/api/reboot", HTTP_POST, handleReboot);
    server.on("/api/wifireset", HTTP_POST, handleWifiReset);
    server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
    server.begin();
    serverStarted = true;
    Serial.println("[WWW] serwer uruchomiony na porcie 80");
}

// ---------------------------------------------------------------- LED

// Pulsowanie kolorem zależnym od stanu:
//   czerwony – Ethernet powinien działać, ale brak kabla/linku
//   żółty    – czas jeszcze nie pobrany z NTP
//   zielony  – godzina w zakresie harmonogramu (Ethernet włączony)
//   niebieski – godzina poza zakresem (Ethernet wyłączony)
static void ledTask() {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last < 20) return;
    last = now;

    uint8_t r = 0, g = 0, b = 0;
    if (ethEnabled && !ethLink) r = 255;
    else if (!timeValid()) { r = 255; g = 160; }
    else if (inSchedule()) g = 255;
    else b = 255;

    float phase = (float)(now % LED_PULSE_MS) / LED_PULSE_MS;
    uint8_t level = led.gamma8((uint8_t)((1.0f - cosf(phase * 2.0f * PI)) * 127.5f));
    uint32_t color = led.Color(r * level / 255, g * level / 255, b * level / 255);
    for (int i = 0; i < LED_COUNT; i++) led.setPixelColor(i, color);
    led.show();
}

// ---------------------------------------------------------------- setup / loop

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== WT32-ETH01, firmware v" FW_VERSION " (" __DATE__ " " __TIME__ ") ===");

    led.begin();
    led.setBrightness(LED_BRIGHTNESS);
    led.clear();
    led.show();

    loadSettings();
    if (customMac && !applyEthMac(savedMac)) {
        Serial.println("[MAC] nie udało się ustawić MAC, używam fabrycznego");
        memcpy(savedMac, defaultMac, 6);
    }
    Serial.printf("Hostname: %s, MAC ETH: %s%s\n", hostname.c_str(), macToString(savedMac).c_str(),
                  customMac ? " (własny)" : " (fabryczny)");

    WiFi.onEvent(onNetEvent);
    WiFi.setHostname(hostname.c_str());

    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE_);

    // SNTP synchronizuje czas przez dowolny interfejs (ETH lub WiFi) i odświeża go co godzinę.
    configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
    Serial.printf("Harmonogram ETH: %s, %s - %s\n", schedEnabled ? "włączony" : "wyłączony",
                  minToStr(ethOnMin).c_str(), minToStr(ethOffMin).c_str());

    // WiFiManager w trybie nieblokującym – Ethernet działa niezależnie od WiFi.
    WiFi.mode(WIFI_STA);
    wm.setHostname(hostname.c_str());
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
    wm.setConnectTimeout(20);
    wm.setTitle("WT32-ETH01");
    String apName = hostname + "-setup";
    if (wm.autoConnect(apName.c_str(), PORTAL_PASSWORD)) {
        Serial.println("[WiFi] połączono");
    } else {
        Serial.printf("[WiFi] portal konfiguracyjny: AP \"%s\", hasło \"%s\", http://192.168.4.1\n",
                      apName.c_str(), PORTAL_PASSWORD);
    }
}

void loop() {
    wm.process();
    scheduleTask();
    ledTask();

    // Portal WiFiManagera zajmuje port 80 – nasz serwer startuje, gdy portal jest nieaktywny.
    if (!serverStarted && !wm.getConfigPortalActive() && !wm.getWebPortalActive()) {
        startServer();
    }
    if (serverStarted) server.handleClient();

    if (!mdnsStarted && (ethGotIP || WiFi.status() == WL_CONNECTED)) {
        if (MDNS.begin(hostname.c_str())) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("[mDNS] http://%s.local\n", hostname.c_str());
        }
        mdnsStarted = true;
    }

    if (restartAt && (int32_t)(millis() - restartAt) >= 0) {
        ESP.restart();
    }
}

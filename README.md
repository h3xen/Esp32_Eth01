# WT32-ETH01 – Ethernet z harmonogramem, WiFiManager i stroną konfiguracyjną

Projekt PlatformIO dla płytki **WT32-ETH01** (ESP32 + PHY LAN8720).
Ustalenia z 2026-10-27.

## Wymagania (ustalone)

1. **WiFiManager** – konfiguracja sieci WiFi telefonem przez punkt dostępowy (captive portal).
2. **Strona WWW** pokazująca:
   - stan Ethernetu (włączony/wyłączony, kabel podłączony/odłączony, IP, prędkość, duplex),
   - aktualny adres MAC Ethernet (oraz fabryczny),
   - stan WiFi (SSID, IP, RSSI), czas pracy, aktualny czas.
3. **Zmiana adresu MAC Ethernet** z poziomu strony, zapamiętywana na stałe.
4. **Zmiana nazwy urządzenia** (hostname) widocznej w sieci, zapamiętywana na stałe.
5. **Skrypt `flash.ps1`** do wgrywania firmware na port **COM3**.
6. **Dioda WS2811 na GPIO4** – sygnalizacja stanu pulsowaniem (pierwotnie losowe kolory, zastąpione przez punkt 8).
7. **Harmonogram Ethernetu** – włączanie o **8:00**, wyłączanie o **16:00** (domyślnie, zmienialne na stronie).
   - czas pobierany z serwera **NTP**,
   - po wyłączeniu Ethernetu czas odmierza zegar ESP32.
8. **Kolory diody (pulsowanie):**

   | Stan | Kolor |
   |---|---|
   | brak kabla / linku Ethernet (gdy Ethernet powinien działać) | 🔴 czerwony |
   | godzina w zakresie od–do (Ethernet włączony) | 🟢 zielony |
   | godzina poza zakresem (Ethernet wyłączony) | 🔵 niebieski |
   | czas jeszcze nie pobrany z NTP *(dodatkowo)* | 🟡 żółty |
9. **Układ strony**: sekcje Harmonogram Ethernetu, Ethernet i WiFi obok siebie (na telefonie jedna pod drugą).
10. **Wgrywanie firmware ze strony** (OTA, plik `.bin`).
11. **Numer wersji firmware** wyświetlany na stronie (`FW_VERSION` + data kompilacji).
12. **Hasło do strony**: `12345678` – bez zalogowania kokpit nie jest widoczny.

## Sprzęt

| Funkcja | Pin / wartość |
|---|---|
| PHY | LAN8720, adres 1 |
| Zasilanie PHY / oscylator | GPIO16 |
| MDC / MDIO | GPIO23 / GPIO18 |
| Zegar RMII | wejście na GPIO0 (`ETH_CLOCK_GPIO0_IN`) |
| Dioda WS2811 | GPIO4 |
| Programowanie | konwerter USB-UART, GPIO0 → GND na czas wgrywania |

## Pliki

| Plik | Opis |
|---|---|
| [platformio.ini](platformio.ini) | `espressif32@6.9.0` (Arduino core 2.0.17), płytka `wt32-eth01`, biblioteki WiFiManager i Adafruit NeoPixel |
| [src/main.cpp](src/main.cpp) | Ethernet, WiFiManager, serwer WWW, NVS, harmonogram, NTP, dioda |
| [src/webpage.h](src/webpage.h) | strona logowania i kokpit (responsywny, tryb ciemny, odświeżanie co 2 s) |
| [flash.ps1](flash.ps1) | kompilacja i wgrywanie |
| [monitor.ps1](monitor.ps1) | monitor portu szeregowego (COM3, 115200, dekoder wyjątków) |

## Wgrywanie

```powershell
.\flash.ps1              # kompilacja + wgranie na COM3
.\flash.ps1 -Monitor     # + monitor portu 115200
.\flash.ps1 -Erase       # najpierw kasuje cały flash (też WiFi, MAC, nazwę, harmonogram)
.\flash.ps1 -Port COM5   # inny port
.\monitor.ps1            # sam monitor portu COM3 @ 115200 (Ctrl+C - wyjście)
```

Przed wgraniem: zewrzeć GPIO0 z GND i zresetować płytkę. Po wgraniu: rozewrzeć i zresetować.
Jeśli PowerShell blokuje skrypty: `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`.

## Obsługa

### Pierwsze uruchomienie / konfiguracja WiFi
- Bez zapisanej sieci startuje AP **`<hostname>-setup`** (domyślnie `wt32-eth01-setup`), hasło **`12345678`**.
- Połączyć się telefonem → portal otworzy się sam (lub http://192.168.4.1) → wybrać sieć.
- Portal zamyka się po **180 s**. Ethernet działa niezależnie od WiFi.

### Strona WWW
- Adres: IP z Ethernetu lub WiFi albo `http://<hostname>.local` (mDNS).
- Logowanie hasłem **`12345678`** (`WEB_PASSWORD`). Sesja w ciasteczku ważna 30 dni, przetrwa restart. Przycisk „Wyloguj”.
- Wersja firmware widoczna w nagłówku i w sekcji „Urządzenie” (razem z datą kompilacji).
- **Aktualizacja firmware**: wybrać `.pio/build/wt32-eth01/firmware.bin` → „Wgraj firmware” → automatyczny restart.
- Zmiana nazwy i MAC → **zapis i restart** urządzenia.
- Zmiana harmonogramu → zapis **bez restartu**, działa od razu.
- „Zmień sieć WiFi” kasuje zapisaną sieć i ponownie uruchamia portal.

### API
| Metoda | Ścieżka | Parametry |
|---|---|---|
| GET | `/api/status` | – (JSON ze stanem) |
| POST | `/api/config` | `hostname`, `mac` |
| POST | `/api/schedule` | `enabled` (0/1), `on`, `off` (HH:MM) |
| POST | `/api/reboot` | – |
| POST | `/api/wifireset` | – |
| POST | `/api/update` | plik `.bin` (multipart) – OTA |
| POST | `/login` | `password` |
| GET | `/logout` | – |

Wszystkie ścieżki `/api/*` wymagają zalogowania (ciasteczko `sid`), w przeciwnym razie zwracają 401.

## Decyzje techniczne i ograniczenia

- **Zmiana MAC Ethernet**: w rdzeniu 2.0.17 MAC Ethernetu = bazowy MAC + 3. Program ustawia
  bazowy MAC = wybrany − 3 (`esp_base_mac_addr_set`) przed startem sieci. Skutek uboczny:
  **MAC WiFi też się przesuwa**. MAC musi być unicast (pierwszy bajt parzysty).
- **Port 80**: portal WiFiManagera i strona używają tego samego portu, więc strona startuje
  dopiero po zamknięciu portalu (przy braku zapisanego WiFi – do 3 min po starcie).
- **Wyłączanie Ethernetu**: `esp_eth_stop()` + uśpienie PHY (rejestr BMCR, bit power-down),
  więc port na switchu gaśnie. Włączanie: wybudzenie PHY + `esp_eth_start()`.
  Zapis rejestru PHY korzysta z wewnętrznej struktury sterownika IDF 4.4 –
  **przy zmianie wersji platformy trzeba to sprawdzić**.
- Gdy Ethernet jest wyłączony, PHY śpi, więc **kabla nie da się wykryć** – dioda świeci wtedy na niebiesko.
- **Czas**: strefa `CET-1CEST,M3.5.0,M10.5.0/3` (Polska, z automatyczną zmianą czasu),
  serwery `pool.ntp.org`, `time.google.com`. SNTP synchronizuje się przez ETH lub WiFi i odświeża co godzinę.
- **Bez zsynchronizowanego czasu Ethernet pozostaje włączony** (jest potrzebny do pobrania czasu).
  Po restarcie poza godzinami pracy Ethernet wstaje, pobiera czas i dopiero wtedy się wyłącza.
- Harmonogram obsługuje przedział przez północ (np. 22:00–06:00). Ta sama godzina włączenia
  i wyłączenia = Ethernet zawsze włączony. Wyłączony harmonogram = Ethernet zawsze włączony.
- Włączenie Ethernetu bez podłączonego kabla może na kilka sekund wstrzymać pętlę programu
  (autonegocjacja PHY).
- **Hasło do strony** jest proste i przesyłane bez szyfrowania (HTTP) – chroni przed przypadkowym dostępem,
  nie przed atakiem w sieci lokalnej. Token sesji jest losowy i zapisany w NVS (`sid`).
- **OTA** korzysta z partycji `min_spiffs.csv` (dwie partycje aplikacji po 1,9 MB). Biblioteka `Update`
  sprawdza nagłówek pliku – zły plik zostanie odrzucony, a stary firmware zostaje.
- **Wersja firmware**: ⚠️ **przy każdym buildzie podbijać** `FW_VERSION` w [src/main.cpp](src/main.cpp) (obecnie 1.2.3).
- **Dioda**: `NEO_GRB + NEO_KHZ800` (sprawdzone na sprzęcie – z `NEO_RGB` zielony świecił na czerwono). Gdy kolory są zamienione → `NEO_RGB`; gdy dioda nie reaguje → `NEO_KHZ400`.

## Ustawienia w kodzie ([src/main.cpp](src/main.cpp))

| Stała | Domyślnie |
|---|---|
| `FW_VERSION` | `1.2.3` |
| `WEB_PASSWORD` | `12345678` |
| `DEFAULT_HOSTNAME` | `wt32-eth01` |
| `PORTAL_PASSWORD` | `12345678` |
| `PORTAL_TIMEOUT_S` | 180 |
| `DEFAULT_ETH_ON_MIN` / `DEFAULT_ETH_OFF_MIN` | 8:00 / 16:00 |
| `TZ_INFO`, `NTP_SERVER1/2` | Polska, pool.ntp.org, time.google.com |
| `LED_PIN`, `LED_COUNT`, `LED_TYPE` | 4, 1, `NEO_GRB + NEO_KHZ800` |
| `LED_BRIGHTNESS`, `LED_PULSE_MS` | 80, 2000 ms |

Ustawienia zmieniane na stronie są zapisywane w NVS (przestrzeń `cfg`: `hostname`, `ethmac`, `sch_en`, `sch_on`, `sch_off`, `sid`).

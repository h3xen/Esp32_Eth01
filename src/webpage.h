#pragma once
#include <Arduino.h>

static const char LOGIN_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Logowanie</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--mut:#6b7385;--bad:#d64545;--acc:#2f6fde;--bd:#dfe2e8}
@media (prefers-color-scheme:dark){:root{--bg:#14171c;--card:#1d2128;--fg:#e6e8ec;--mut:#9099a8;--bd:#2c313a}}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:16px}
form{width:100%;max-width:320px;background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:20px}
h1{font-size:18px;margin:0 0 14px}
input{width:100%;padding:10px;border:1px solid var(--bd);border-radius:7px;background:var(--bg);color:var(--fg);font-size:15px}
button{width:100%;margin-top:12px;padding:10px;border:0;border-radius:7px;background:var(--acc);color:#fff;font-size:15px;cursor:pointer}
#err{color:var(--bad);min-height:1.4em;margin-top:8px}
</style>
</head>
<body>
<form method="post" action="/login">
<h1>WT32-ETH01</h1>
<input type="password" name="password" placeholder="Hasło" autofocus required>
<button type="submit">Zaloguj</button>
<div id="err"></div>
</form>
<script>if(location.search.indexOf('err=1')>=0)document.getElementById('err').textContent='Błędne hasło.';</script>
</body>
</html>
)HTML";

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WT32-ETH01</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--mut:#6b7385;--ok:#1f9d55;--bad:#d64545;--acc:#2f6fde;--bd:#dfe2e8}
@media (prefers-color-scheme:dark){:root{--bg:#14171c;--card:#1d2128;--fg:#e6e8ec;--mut:#9099a8;--bd:#2c313a}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:1180px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:4px 0 16px}
.grid{display:grid;grid-template-columns:1fr;gap:14px;margin-bottom:14px}
@media (min-width:900px){.grid{grid-template-columns:repeat(3,minmax(0,1fr))}}
.card{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:14px 16px;display:flex;flex-direction:column;min-width:0}
.card h2{font-size:15px;margin:0 0 10px;color:var(--mut);text-transform:uppercase;letter-spacing:.04em}
.row{display:flex;justify-content:space-between;gap:12px;padding:5px 0;border-bottom:1px dashed var(--bd)}
.row:last-child{border-bottom:0}
.row span:first-child{color:var(--mut)}
.row span:last-child{font-family:ui-monospace,Consolas,monospace;text-align:right;word-break:break-all}
.badge{display:inline-block;padding:1px 10px;border-radius:99px;color:#fff;font-family:system-ui,sans-serif!important}
.ok{background:var(--ok)}.bad{background:var(--bad)}
label{display:block;margin:10px 0 4px;color:var(--mut)}
input,select{width:100%;padding:9px 10px;border:1px solid var(--bd);border-radius:7px;background:var(--bg);color:var(--fg);font:15px ui-monospace,Consolas,monospace}
.card form{display:flex;flex-direction:column;flex:1}
.btns{display:flex;flex-wrap:wrap;gap:8px;margin-top:auto;padding-top:12px}
button{padding:9px 14px;border:0;border-radius:7px;background:var(--acc);color:#fff;font-size:14px;cursor:pointer}
button:disabled{opacity:.5;cursor:default}
button.sec{background:transparent;color:var(--acc);border:1px solid var(--acc)}
button.dng{background:var(--bad)}
small{color:var(--mut)}
.msg{min-height:1.4em;margin-top:8px}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
label.chk{display:flex;align-items:center;gap:8px;color:var(--fg)}
label.chk input{width:auto}
.prof{border-top:1px dashed var(--bd);margin-top:12px;padding-top:2px}
progress{width:100%;height:14px;margin-top:10px;accent-color:var(--acc)}
</style>
</head>
<body>
<main>
<h1><span id="title">WT32-ETH01</span> <small id="h1fw"></small></h1>

<div class="grid">
<div class="card">
<h2>Harmonogram Ethernetu</h2>
<div class="row"><span>Aktualny czas</span><span id="time">-</span></div>
<div class="row"><span>Teraz w zakresie</span><span id="in_range">-</span></div>
<form id="fs">
<label class="chk"><input type="checkbox" id="sch_en"> Harmonogram aktywny</label>
<div class="two">
<div><label for="sch_on">Włącz Ethernet o</label><input type="time" id="sch_on" required></div>
<div><label for="sch_off">Wyłącz Ethernet o</label><input type="time" id="sch_off" required></div>
</div>
<small>Czas z serwera NTP; po wyłączeniu Ethernetu liczy go zegar ESP32. Bez czasu Ethernet pozostaje włączony.</small>
<div class="btns"><button type="submit">Zapisz harmonogram</button></div>
</form>
<div class="msg" id="msg_sch"></div>
</div>

<div class="card">
<h2>Ethernet</h2>
<div class="row"><span>Interfejs</span><span id="eth_en">-</span></div>
<div class="row"><span>Kabel / link</span><span id="eth_link">-</span></div>
<div class="row"><span>Adres IP</span><span id="eth_ip">-</span></div>
<div class="row"><span>Prędkość</span><span id="eth_speed">-</span></div>
<div class="row"><span>MAC (aktualny)</span><span id="eth_mac">-</span></div>
<div class="row"><span>MAC (fabryczny)</span><span id="eth_mac_def">-</span></div>
</div>

<div class="card">
<h2>WiFi</h2>
<div class="row"><span>Status</span><span id="wifi_st">-</span></div>
<div class="row"><span>SSID</span><span id="wifi_ssid">-</span></div>
<div class="row"><span>Adres IP</span><span id="wifi_ip">-</span></div>
<div class="row"><span>RSSI</span><span id="wifi_rssi">-</span></div>
</div>
</div>

<div class="grid">
<div class="card">
<h2>Ustawienia</h2>
<form id="f">
<label for="hostname">Nazwa urządzenia (hostname)</label>
<input id="hostname" name="hostname" maxlength="32" required pattern="[A-Za-z0-9]([A-Za-z0-9\-]{0,30}[A-Za-z0-9])?">
<small>Litery, cyfry i myślnik, max 32 znaki. Dostępne też jako <i>nazwa</i>.local</small>
<label for="mac">Adres MAC Ethernet</label>
<input id="mac" name="mac" maxlength="17" required placeholder="AA:BB:CC:DD:EE:FF">
<small>Pierwszy bajt musi być parzysty (adres unicast).</small>
<div class="btns">
<button type="submit">Zapisz i uruchom ponownie</button>
<button type="button" class="sec" id="macdef">Przywróć fabryczny MAC</button>
</div>
</form>
<div class="msg" id="msg"></div>
<div class="prof">
<label for="pr_label">Podpis profilu</label>
<input id="pr_label" maxlength="32" placeholder="np. Biuro – switch port 12">
<small>„Zapisz profil” zapamiętuje powyższą nazwę i MAC pod tym podpisem (max 10 profili).</small>
<label for="pr_sel">Zapisane profile</label>
<select id="pr_sel"></select>
<div class="btns">
<button type="button" class="sec" id="pr_save">Zapisz profil</button>
<button type="button" id="pr_load">Wczytaj</button>
<button type="button" class="sec" id="pr_del">Usuń</button>
</div>
<div class="msg" id="msg_pr"></div>
</div>
</div>

<div class="card">
<h2>Aktualizacja firmware</h2>
<form id="fu">
<label for="fw">Plik firmware (.bin)</label>
<input type="file" id="fw" accept=".bin" required>
<small>Plik po kompilacji: <i>.pio/build/wt32-eth01/firmware.bin</i></small>
<progress id="fw_prog" max="100" value="0" hidden></progress>
<div class="btns"><button type="submit" id="fw_btn">Wgraj firmware</button></div>
</form>
<div class="msg" id="msg_fw"></div>
</div>

<div class="card">
<h2>Urządzenie</h2>
<div class="row"><span>Nazwa w sieci</span><span id="host">-</span></div>
<div class="row"><span>Czas pracy</span><span id="uptime">-</span></div>
<div class="row"><span>Wersja firmware</span><span id="fwver">-</span></div>
<div class="row"><span>Kompilacja</span><span id="fwbuild">-</span></div>
<small style="margin-top:10px">„Zmień sieć WiFi” kasuje zapisaną sieć i uruchamia punkt dostępowy konfiguracji.</small>
<div class="btns">
<button type="button" class="sec" id="logout">Wyloguj</button>
<button type="button" class="sec" id="reboot">Restart</button>
<button type="button" class="dng" id="wifireset">Zmień sieć WiFi</button>
</div>
<div class="msg" id="msg_dev"></div>
</div>
</div>
</main>

<script>
const $=id=>document.getElementById(id);
let formFilled=false,schedFilled=false;
function badge(ok,t,f){return '<span class="badge '+(ok?'ok':'bad')+'">'+(ok?t:f)+'</span>'}
function fmtUp(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return (d?d+'d ':'')+h+'h '+m+'m '+(s%60)+'s'}
function say(out,ok,t){const m=$(out);m.textContent=t;m.style.color=ok?'var(--ok)':'var(--bad)'}
async function refresh(){
  try{
    const r=await fetch('/api/status');if(r.status==401){location.reload();return}const j=await r.json();
    $('title').textContent=j.hostname;document.title=j.hostname;
    $('time').textContent=j.time_valid?j.time:'brak synchronizacji NTP';
    $('in_range').innerHTML=j.time_valid?badge(j.sched.in_range,'Tak','Nie'):'-';
    if(!schedFilled){$('sch_en').checked=j.sched.enabled;$('sch_on').value=j.sched.on;$('sch_off').value=j.sched.off;schedFilled=true}
    $('eth_en').innerHTML=badge(j.eth.enabled,'Włączony','Wyłączony (harmonogram)');
    $('eth_link').innerHTML=j.eth.enabled?badge(j.eth.link,'Podłączony','Odłączony'):'-';
    $('eth_ip').textContent=j.eth.ip||'-';
    $('eth_speed').textContent=j.eth.link?(j.eth.speed+' Mb/s '+(j.eth.duplex?'Full':'Half')+' duplex'):'-';
    $('eth_mac').textContent=j.eth.mac;
    $('eth_mac_def').textContent=j.eth.mac_default;
    $('wifi_st').innerHTML=badge(j.wifi.connected,'Połączony','Rozłączony');
    $('wifi_ssid').textContent=j.wifi.ssid||'-';
    $('wifi_ip').textContent=j.wifi.ip||'-';
    $('wifi_rssi').textContent=j.wifi.connected?j.wifi.rssi+' dBm':'-';
    $('host').textContent=j.hostname;
    $('uptime').textContent=fmtUp(j.uptime);
    $('fwver').textContent='v'+j.fw;$('fwbuild').textContent=j.build;$('h1fw').textContent='v'+j.fw;
    if(!formFilled){$('hostname').value=j.hostname;$('mac').value=j.eth.mac_saved;$('macdef').dataset.mac=j.eth.mac_default;formFilled=true}
  }catch(e){}
}
async function post(url,data,out){
  const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data||{})});
  if(r.status==401){location.reload();return false}
  say(out,r.ok,await r.text());return r.ok;
}
$('logout').onclick=()=>{location.href='/logout'};
$('fs').onsubmit=async e=>{e.preventDefault();
  await post('/api/schedule',{enabled:$('sch_en').checked?'1':'0',on:$('sch_on').value,off:$('sch_off').value},'msg_sch');
};
$('f').onsubmit=async e=>{e.preventDefault();
  if(await post('/api/config',{hostname:$('hostname').value.trim(),mac:$('mac').value.trim()},'msg'))
    setTimeout(()=>location.reload(),8000);
};
let profs=[];
async function loadProfs(sel){
  const r=await fetch('/api/profiles');if(r.status==401){location.reload();return}
  profs=await r.json();const s=$('pr_sel');s.innerHTML='';
  if(!profs.length)s.add(new Option('(brak zapisanych profili)',''));
  profs.forEach((p,i)=>s.add(new Option(p.label+' — '+p.hostname+' / '+p.mac,i)));
  const i=profs.findIndex(p=>p.label==sel);if(i>=0)s.value=i;
  $('pr_load').disabled=$('pr_del').disabled=!profs.length;
}
$('pr_save').onclick=async()=>{const l=$('pr_label').value.trim();
  if(!l){say('msg_pr',false,'Podaj podpis profilu.');return}
  if(profs.some(p=>p.label==l)&&!confirm('Nadpisać profil „'+l+'”?'))return;
  if(await post('/api/profiles/save',{label:l,hostname:$('hostname').value.trim(),mac:$('mac').value.trim()},'msg_pr'))loadProfs(l)};
$('pr_load').onclick=()=>{const p=profs[$('pr_sel').value];if(!p)return;
  $('hostname').value=p.hostname;$('mac').value=p.mac;$('pr_label').value=p.label;
  say('msg_pr',true,'Wczytano „'+p.label+'”. Kliknij „Zapisz i uruchom ponownie”, aby zastosować.')};
$('pr_del').onclick=async()=>{const p=profs[$('pr_sel').value];
  if(!p||!confirm('Usunąć profil „'+p.label+'”?'))return;
  if(await post('/api/profiles/delete',{label:p.label},'msg_pr'))loadProfs()};
$('macdef').onclick=()=>{$('mac').value=$('macdef').dataset.mac||''};
$('fu').onsubmit=e=>{e.preventDefault();
  const f=$('fw').files[0];if(!f)return;
  if(!confirm('Wgrać '+f.name+' ('+Math.round(f.size/1024)+' kB)?'))return;
  const fd=new FormData();fd.append('firmware',f,f.name);
  const x=new XMLHttpRequest(),p=$('fw_prog');
  p.hidden=false;p.value=0;$('fw_btn').disabled=true;say('msg_fw',true,'Wysyłanie...');
  x.upload.onprogress=ev=>{if(ev.lengthComputable){p.value=ev.loaded*100/ev.total;say('msg_fw',true,'Wysyłanie... '+Math.round(p.value)+'%')}};
  x.onload=()=>{const ok=x.status==200;say('msg_fw',ok,x.responseText);$('fw_btn').disabled=false;
    if(ok)setTimeout(()=>location.reload(),10000)};
  x.onerror=()=>{say('msg_fw',false,'Błąd połączenia podczas wysyłania.');$('fw_btn').disabled=false};
  x.open('POST','/api/update');x.send(fd);
};
$('reboot').onclick=()=>{if(confirm('Zrestartować urządzenie?'))post('/api/reboot',null,'msg_dev').then(()=>setTimeout(()=>location.reload(),8000))};
$('wifireset').onclick=()=>{if(confirm('Skasować zapisaną sieć WiFi i uruchomić portal konfiguracyjny?'))post('/api/wifireset',null,'msg_dev')};
refresh();setInterval(refresh,2000);loadProfs();
</script>
</body>
</html>
)HTML";

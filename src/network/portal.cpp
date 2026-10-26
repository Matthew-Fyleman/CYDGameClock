/*
 * CYD Game Clock - WiFi Setup Portal
 */

#include "portal.h"
#include "network.h"
#include "../ui/ui.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_random.h>
#include <esp_task_wdt.h>

static WebServer portalServer(80);
static DNSServer dns;

enum TestState { TEST_IDLE, TEST_RUNNING, TEST_OK, TEST_FAILED };
static TestState testState = TEST_IDLE;
static unsigned long testStarted = 0;
static unsigned long restartAt = 0;
static String testSsid, testPass, testError;

// Why the router turned the clock away, from the WiFi driver's events
static volatile uint8_t lastDisconnectReason = 0;
static volatile uint8_t disconnectCount = 0;
static volatile bool associated = false;  // joined the network (before getting an IP address)

static const char PORTAL_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>CYD Game Clock WiFi Setup</title>
<style>*{box-sizing:border-box}body{font-family:-apple-system,Segoe UI,Arial,sans-serif;margin:0;padding:18px;
background:#0f0c29;color:#e0e7ff}.c{max-width:420px;margin:0 auto}h1{font-size:24px;text-align:center;color:#fff}
p{color:#94a3b8;font-size:14px;line-height:1.4}.card{background:#16213e;border:1px solid #1e3a5f;border-radius:12px;padding:16px;margin:14px 0}
.net{display:flex;justify-content:space-between;align-items:center;padding:12px;border-radius:8px;background:#0f3460;margin:6px 0;cursor:pointer;font-size:16px}
.net:hover,.net.sel{background:#1a5a9a}.bars{color:#00d4ff;font-size:13px}label{display:block;margin:12px 0 6px;color:#00d4ff;font-size:14px}
input{width:100%;padding:12px;border-radius:8px;border:2px solid #1e3a5f;background:#0f3460;color:#fff;font-size:16px}
button{width:100%;padding:14px;margin-top:16px;border:none;border-radius:8px;font-size:16px;font-weight:600;background:#00d4ff;color:#0f0c29;cursor:pointer}
button.sec{background:#334155;color:#fff}.msg{padding:12px;border-radius:8px;margin-top:14px;display:none}.ok{background:#065f46}.err{background:#7f1d1d}.wait{background:#1e3a5f}
.show{display:flex;align-items:center;gap:8px;margin-top:8px;font-size:14px;color:#94a3b8}.show input{width:auto}</style></head><body><div class="c">
<h1>&#128377; CYD Game Clock</h1><p>Choose your home WiFi network and enter its password. The clock checks the password before saving it.</p>
<div class="card"><div id="nets"><p>Searching for networks&hellip;</p></div><button class="sec" onclick="scan(1)">Search again</button></div>
<div class="card"><label for="ssid">Network name</label><input id="ssid" autocomplete="off" autocapitalize="none">
<label for="pass">Password</label><input id="pass" type="password" autocomplete="off">
<div class="show"><input type="checkbox" id="sp" onchange="pass.type=this.checked?'text':'password'"><span>Show password</span></div>
<button id="go" onclick="go()">Connect</button><div id="msg" class="msg"></div></div></div>
<script>
function bars(r){return r>-55?'&#9679;&#9679;&#9679;&#9679;':r>-65?'&#9679;&#9679;&#9679;&#9675;':r>-75?'&#9679;&#9679;&#9675;&#9675;':'&#9679;&#9675;&#9675;&#9675;'}
function scan(again){fetch('/scan'+(again?'?again=1':'')).then(r=>r.json()).then(d=>{if(d.scanning){setTimeout(scan,1500);return}
const n=document.getElementById('nets');n.innerHTML='';if(!d.networks.length){n.innerHTML='<p>No networks found.</p>'}
d.networks.forEach(w=>{const e=document.createElement('div');e.className='net';const s=document.createElement('span');s.textContent=(w.secure?'\u{1F512} ':'')+w.ssid;
const b=document.createElement('span');b.className='bars';b.innerHTML=bars(w.rssi);e.appendChild(s);e.appendChild(b);
e.onclick=()=>{document.querySelectorAll('.net').forEach(x=>x.classList.remove('sel'));e.classList.add('sel');ssid.value=w.ssid;pass.focus()};n.appendChild(e)})}).catch(()=>setTimeout(scan,2000))}
function show(c,t){const m=document.getElementById('msg');m.className='msg '+c;m.style.display='block';m.textContent=t}
function go(){if(!ssid.value){show('err','Please choose or type a network name.');return}
document.getElementById('go').disabled=true;show('wait','Connecting to '+ssid.value+'… this can take up to 30 seconds.');
const f=new URLSearchParams();f.append('ssid',ssid.value);f.append('pass',pass.value);
fetch('/connect',{method:'POST',body:f}).then(()=>setTimeout(poll,1500)).catch(()=>setTimeout(poll,1500))}
function poll(){fetch('/status').then(r=>r.json()).then(d=>{if(d.state=='running'){setTimeout(poll,1000)}
else if(d.state=='ok'){show('ok','Connected! The clock\'s address on your network is '+d.ip+'. It is saving the settings and restarting - you can close this page and reconnect your phone to your home WiFi.')}
else{document.getElementById('go').disabled=false;show('err','Could not connect: '+d.error)}})
.catch(()=>setTimeout(poll,1500))}
scan(0);
</script></body></html>)rawliteral";

static String makeApPassword() {
  static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
  String p;
  for (int i = 0; i < 8; i++) p += alphabet[esp_random() % (sizeof(alphabet) - 1)];
  return p;
}

static void redirectToPortal() {
  portalServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  portalServer.send(302, "text/plain", "");
}

static void handleScan() {
  if (portalServer.hasArg("again")) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  }
  int n = WiFi.scanComplete();
  JsonDocument doc;
  if (n == WIFI_SCAN_RUNNING) {
    doc["scanning"] = true;
  } else {
    if (n < 0) {
      WiFi.scanNetworks(true);
      doc["scanning"] = true;
    } else {
      JsonArray arr = doc["networks"].to<JsonArray>();
      for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        bool duplicate = false;  // mesh networks list the same name many times
        for (JsonObject o : arr) {
          if (ssid == o["ssid"].as<const char*>()) duplicate = true;
        }
        if (duplicate) continue;
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = ssid;
        o["rssi"] = WiFi.RSSI(i);
        o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      }
    }
  }
  String out;
  serializeJson(doc, out);
  portalServer.send(200, "application/json", out);
}

static void handleConnect() {
  testSsid = portalServer.arg("ssid");
  testPass = portalServer.arg("pass");
  if (testSsid.isEmpty() || testSsid.length() > 32 || testPass.length() > 64) {
    portalServer.send(400, "text/plain", "bad request");
    return;
  }
  portalServer.send(200, "text/plain", "ok");

  // A scan still running would make the connection attempt fail at once
  while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) delay(50);
  WiFi.scanDelete();

  WiFi.disconnect(false);
  delay(100);
  lastDisconnectReason = 0;
  disconnectCount = 0;
  associated = false;
  WiFi.begin(testSsid.c_str(), testPass.c_str());
  testState = TEST_RUNNING;
  testStarted = millis();
  uiShowPortalStatus(("Trying " + testSsid + "...").c_str(), 0xFFE0);
}

static void handleStatus() {
  JsonDocument doc;
  switch (testState) {
    case TEST_RUNNING: doc["state"] = "running"; break;
    case TEST_OK:
      doc["state"] = "ok";
      doc["ip"] = WiFi.localIP().toString();
      break;
    case TEST_FAILED:
      doc["state"] = "failed";
      doc["error"] = testError;
      break;
    default: doc["state"] = "idle"; break;
  }
  String out;
  serializeJson(doc, out);
  portalServer.send(200, "application/json", out);
}

static void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    if (info.wifi_sta_disconnected.reason == WIFI_REASON_ASSOC_LEAVE) return;  // our own disconnect()
    lastDisconnectReason = info.wifi_sta_disconnected.reason;
    disconnectCount++;
    associated = false;
  } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    associated = true;
  }
}

// Plain-language explanation of an ESP32 disconnect reason code
static String explainReason(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
      return "the network was not found (the clock only sees 2.4 GHz WiFi).";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return "the router did not accept the password.";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_NOT_AUTHED:
    case WIFI_REASON_NOT_ASSOCED:
    case WIFI_REASON_ASSOC_NOT_AUTHED:
      return "the router refused to let the clock join (it may block new devices, or allow only listed ones).";
    case WIFI_REASON_IE_INVALID:
    case WIFI_REASON_INVALID_RSN_IE_CAP:
    case WIFI_REASON_AKMP_INVALID:
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
    case WIFI_REASON_GROUP_CIPHER_INVALID:
      return "the router's security mode is not supported (try WPA2 or WPA2/WPA3 mixed mode instead of WPA3 only).";
    default:
      return "the router did not answer.";
  }
}

static void checkConnectionTest() {
  if (testState != TEST_RUNNING) return;
  wl_status_t st = WiFi.status();
  unsigned long elapsed = millis() - testStarted;

  if (st == WL_CONNECTED) {
    testState = TEST_OK;
    wifiCredsSave(testSsid, testPass);
    Serial.printf("Portal: connected to '%s', credentials saved\n", testSsid.c_str());
    uiShowPortalStatus(("Connected! IP " + WiFi.localIP().toString() + " - restarting").c_str(), 0x07E0);
    restartAt = millis() + 8000;  // give the phone time to read the result
    return;
  }

  // The driver retries by itself; give up after three refusals or 30 s
  if (disconnectCount >= 3 || elapsed > 30000) {
    testState = TEST_FAILED;
    uint8_t reason = lastDisconnectReason;
    if (associated) {
      testError = "the clock joined the network but the router did not give it an address. "
                  "Router security may be holding new devices for approval - check your router or its app.";
    } else if (reason) {
      testError = explainReason(reason) + " (code " + String(reason) + ")";
    } else {
      testError = "the router did not answer.";
    }
    Serial.printf("Portal: could not join '%s': reason %d, associated %d, status %d\n",
                  testSsid.c_str(), reason, associated, st);
    WiFi.disconnect(false);
    uiShowPortalStatus(("Could not join " + testSsid + " - code " + String(reason)).c_str(), 0xF800);
  }
}

void runProvisioningPortal() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char apName[32];
  snprintf(apName, sizeof(apName), "%s%02X%02X", AP_NAME_PREFIX, mac[4], mac[5]);
  String apPass = makeApPassword();

  WiFi.persistent(false);
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName, apPass.c_str());
  delay(200);
  IPAddress apIP = WiFi.softAPIP();
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", apIP);
  WiFi.scanNetworks(true);

  portalServer.on("/", []() { portalServer.send_P(200, "text/html", PORTAL_PAGE); });
  portalServer.on("/scan", handleScan);
  portalServer.on("/connect", HTTP_POST, handleConnect);
  portalServer.on("/status", handleStatus);
  portalServer.onNotFound(redirectToPortal);  // captive-portal checks land here
  portalServer.begin();

  Serial.printf("Setup portal: join '%s' (password %s) and open http://%s\n", apName,
                apPass.c_str(), apIP.toString().c_str());
  uiShowPortal(apName, apPass.c_str(), apIP.toString().c_str());

  while (true) {
    esp_task_wdt_reset();
    dns.processNextRequest();
    portalServer.handleClient();
    checkConnectionTest();
    if (restartAt && millis() > restartAt) ESP.restart();
    delay(5);
  }
}

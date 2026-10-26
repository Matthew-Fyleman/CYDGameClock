/*
 * CYD Game Clock - Network Module
 *
 * WiFi connection management, encrypted UDP and serial stats links, NTP sync.
 */

#include "network.h"
#include "security.h"
#include "../ui/ui.h"
#include "../utils/utils.h"
#include <ESPmDNS.h>
#include <Preferences.h>

// Global network objects
WiFiUDP udp;

// ========== Stored WiFi Credentials ==========
bool wifiCredsLoad(String& ssid, String& pass) {
  Preferences prefs;
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return ssid.length() > 0;
}

void wifiCredsSave(const String& ssid, const String& pass) {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void wifiCredsClear() {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  WiFi.disconnect(true, true);
}

// ========== Static IP Application ==========
static void applyStaticIP() {
  if (!settings.useStaticIP) return;

  IPAddress local_IP, gateway_IP, subnet_IP, dns1_IP, dns2_IP;
  if (local_IP.fromString(settings.staticIP) &&
      gateway_IP.fromString(settings.gateway) &&
      subnet_IP.fromString(settings.subnet) &&
      dns1_IP.fromString(settings.dns1)) {
    dns2_IP.fromString(settings.dns2);
    Serial.printf("Configuring Static IP %s\n", settings.staticIP);
    if (!WiFi.config(local_IP, gateway_IP, subnet_IP, dns1_IP, dns2_IP)) {
      Serial.println("Static IP configuration failed!");
    }
  } else {
    Serial.println("Invalid static IP configuration, using DHCP");
  }
}

// ========== WiFi Connection ==========
bool connectWiFi(const String& ssid, const String& pass) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.setAutoReconnect(true);
  applyStaticIP();
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  int lastShown = -1;
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
    int secs = (millis() - start) / 1000;
    if (secs != lastShown) {
      lastShown = secs;
      uiShowConnecting(ssid.c_str(), secs, WIFI_CONNECT_TIMEOUT / 1000);
    }
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected, IP %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("WiFi connection timed out - will keep retrying in the background");
  return false;
}

void startNetworkServices() {
  static bool started = false;
  if (started) return;
  started = true;

  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  udp.begin(UDP_PORT);
  Serial.printf("UDP listening on port %d\n", UDP_PORT);

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("cydgameclock", "udp", UDP_PORT);
    Serial.println("mDNS: http://" MDNS_HOSTNAME ".local");
  }
}

// ========== NTP Functions ==========
void applyTimezone() {
  int gmtOffset_sec = settings.gmtOffset * 60;  // Convert minutes to seconds
  int daylightOffset_sec = settings.daylightSaving ? 3600 : 0;

  configTime(gmtOffset_sec, daylightOffset_sec, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
}

void initNTP() {
  applyTimezone();
  ntpSynced = false;

  uiShowMessage("Syncing time...", nullptr, nullptr);

  struct tm timeinfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeinfo, 100)) {
      if (timeinfo.tm_year > 120) {
        ntpSynced = true;
        lastNtpSyncTime = millis();
        Serial.println("NTP time synchronized successfully");
        break;
      }
    }
    delay(100);
  }

  if (!ntpSynced) {
    Serial.println("NTP sync pending, will retry in background");
  }
}

// ========== WiFi Reconnection Handling ==========
// Reconnection interval in milliseconds (try every 30 seconds)
#define WIFI_RECONNECT_INTERVAL 30000

void handleWiFiReconnection() {
  static unsigned long lastReconnectAttempt = 0;

  if (WiFi.status() != WL_CONNECTED) {
    // Update global flag for icon display
    wifiConnected = false;

    if (wifiDisconnectTime == 0) {
      wifiDisconnectTime = millis();
      Serial.println("WiFi disconnected");
    }

    // Periodic reconnection attempt every 30 seconds
    unsigned long currentMillis = millis();
    if (currentMillis - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      Serial.println("Attempting WiFi reconnection...");
      WiFi.reconnect();
      lastReconnectAttempt = currentMillis;
    }
  } else {
    // WiFi is connected
    wifiConnected = true;
    startNetworkServices();

    if (wifiDisconnectTime != 0) {
      Serial.println("WiFi reconnected successfully!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      wifiDisconnectTime = 0;
      ntpSynced = false;  // Force NTP resync after reconnection
    }
  }
}

// ========== Reply to the daemon ==========
// Lists the metric IDs the screen actually uses, so the daemon can skip
// reading sensors nobody looks at.
static void buildAck(JsonDocument& ack, uint32_t sid, uint32_t seq) {
  ack["dev"] = "CYDGameClock";
  ack["fw"] = FIRMWARE_VERSION;
  ack["sid"] = sid;
  ack["ack"] = seq;
  JsonArray want = ack["want"].to<JsonArray>();
  for (int i = 0; i < MAX_METRICS; i++) {
    bool used = settings.metricPositions[i] != 255 || settings.metricBarPositions[i] != 255;
    if (!used) {
      // Also wanted when it is the companion of a visible metric
      for (int j = 0; j < MAX_METRICS && !used; j++) {
        used = settings.metricCompanions[j] == i + 1 &&
               (settings.metricPositions[j] != 255 || settings.metricBarPositions[j] != 255);
      }
    }
    if (used) want.add(i + 1);
  }
}

// ========== UDP Packet Handling ==========
void handleUDP() {
  if (!wifiConnected) return;
  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  static uint8_t packet[2048];
  static char plain[2048];
  if (packetSize > (int)sizeof(packet)) {
    Serial.printf("UDP packet of %d bytes is too big - dropped\n", packetSize);
    udp.flush();
    return;
  }
  int len = udp.read(packet, sizeof(packet));
  if (len <= 0) return;

  int plainLen = decryptPacket(packet, len, plain, sizeof(plain));
  if (plainLen < 0) {
    metricData.rejected++;
    Serial.println("UDP packet rejected (not encrypted with this clock's pairing code)");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, plain, plainLen)) {
    metricData.rejected++;
    return;
  }

  uint32_t sid = doc["sid"] | 0u;
  uint32_t seq = doc["seq"] | 0u;
  uint32_t t = doc["t"] | 0u;
  if (!acceptPacketCounter(sid, seq, t, ntpSynced)) {
    metricData.rejected++;
    Serial.println("UDP packet rejected (replayed or clock skew too large)");
    return;
  }

  parseStatsV2(doc);
  metricData.link = LINK_WIFI;
  lastReceived = millis();

  JsonDocument ack;
  buildAck(ack, sid, seq);
  char ackJson[256];
  size_t ackLen = serializeJson(ack, ackJson, sizeof(ackJson));
  uint8_t ackPacket[256 + PACKET_OVERHEAD];
  int n = encryptPacket(ackJson, ackLen, ackPacket, sizeof(ackPacket));
  if (n > 0) {
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(ackPacket, n);
    udp.endPacket();
  }
}

// ========== Serial Link ==========
// The daemon probes each COM port with "?CYDGC" and this clock answers
// "!CYDGC <mac> <version>"; stats then arrive as one JSON object per line.
void handleSerial() {
  static char line[2048];
  static size_t used = 0;
  static bool overflow = false;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (used < sizeof(line) - 1) {
        line[used++] = c;
      } else {
        overflow = true;
      }
      continue;
    }

    line[used] = '\0';
    if (overflow) {
      Serial.println("!ERR line too long");
    } else if (strcmp(line, "?CYDGC") == 0) {
      Serial.printf("!CYDGC %s %s\n", WiFi.macAddress().c_str(), FIRMWARE_VERSION);
    } else if (line[0] == '{') {
      JsonDocument doc;
      if (deserializeJson(doc, line, used) == DeserializationError::Ok) {
        parseStatsV2(doc);
        metricData.link = LINK_SERIAL;
        lastReceived = millis();

        JsonDocument ack;
        buildAck(ack, doc["sid"] | 0u, doc["seq"] | 0u);
        Serial.print("!ACK ");
        serializeJson(ack, Serial);
        Serial.println();
      } else {
        Serial.println("!ERR bad json");
      }
    }
    used = 0;
    overflow = false;
  }
}

// ========== Stats Parsing ==========
// Apply the per-ID layout settings to a metric
static void applyMetricSettings(Metric& m, uint8_t defaultOrder) {
  if (m.id > 0 && m.id <= MAX_METRICS) {
    bool nameMatches = (settings.metricNames[m.id - 1][0] == '\0' ||
                        strcmp(settings.metricNames[m.id - 1], m.name) == 0);

    if (nameMatches) {
      if (settings.metricLabels[m.id - 1][0] != '\0') {
        strncpy(m.label, settings.metricLabels[m.id - 1], METRIC_NAME_LEN - 1);
      } else {
        strncpy(m.label, m.name, METRIC_NAME_LEN - 1);
      }
      m.label[METRIC_NAME_LEN - 1] = '\0';

      m.displayOrder = settings.metricOrder[m.id - 1];
      m.companionId = settings.metricCompanions[m.id - 1];
      m.position = settings.metricPositions[m.id - 1];
      m.barPosition = settings.metricBarPositions[m.id - 1];
      m.barMin = settings.metricBarMin[m.id - 1];
      m.barMax = settings.metricBarMax[m.id - 1];
      m.barWidth = settings.metricBarWidths[m.id - 1];
      m.barOffsetX = settings.metricBarOffsets[m.id - 1];

      strncpy(settings.metricNames[m.id - 1], m.name, METRIC_NAME_LEN - 1);
      settings.metricNames[m.id - 1][METRIC_NAME_LEN - 1] = '\0';
      return;
    }

    Serial.printf("Metric ID %d name changed: '%s' -> '%s', using defaults\n",
                  m.id, settings.metricNames[m.id - 1], m.name);
    strncpy(settings.metricNames[m.id - 1], m.name, METRIC_NAME_LEN - 1);
    settings.metricNames[m.id - 1][METRIC_NAME_LEN - 1] = '\0';
    settings.metricLabels[m.id - 1][0] = '\0';
  }

  strncpy(m.label, m.name, METRIC_NAME_LEN - 1);
  m.label[METRIC_NAME_LEN - 1] = '\0';
  m.displayOrder = defaultOrder;
  m.companionId = 0;
  m.position = 255;
  m.barPosition = 255;
  m.barMin = 0;
  m.barMax = 100;
  m.barWidth = 60;
  m.barOffsetX = 0;
}

static void readMetric(JsonObject metricObj, Metric& m) {
  m.id = metricObj["id"] | 0;

  const char* name = metricObj["name"];
  if (name) {
    strncpy(m.name, name, METRIC_NAME_LEN - 1);
    m.name[METRIC_NAME_LEN - 1] = '\0';
    trimTrailingSpaces(m.name);
  }

  const char* unit = metricObj["unit"];
  if (unit) {
    strncpy(m.unit, unit, METRIC_UNIT_LEN - 1);
    m.unit[METRIC_UNIT_LEN - 1] = '\0';
  }

  m.value = metricObj["value"] | 0;
}

void parseStatsV2(JsonDocument& doc) {
  // Parse status code (1=OK, 2=API error, 3=LHM not running, etc.)
  uint8_t newStatus = doc["status"] | STATUS_OK;
  if (newStatus != metricData.status) {
    Serial.printf("PC status: %d\n", newStatus);
  }
  metricData.status = newStatus;

  const char* ts = doc["timestamp"];
  if (ts && strlen(ts) > 0) {
    strncpy(metricData.timestamp, ts, 5);
    metricData.timestamp[5] = '\0';
  }
  // An empty timestamp means stale data from the daemon - keep the last one

  JsonArray metricsArray = doc["metrics"];
  bool partial = doc["partial"] | false;

  if (partial && metricData.count > 0) {
    // Only the metrics on screen were sent: update those, keep the rest
    for (JsonObject metricObj : metricsArray) {
      uint8_t id = metricObj["id"] | 0;
      for (int i = 0; i < metricData.count; i++) {
        Metric& m = metricData.metrics[i];
        if (m.id == id) {
          readMetric(metricObj, m);
          applyMetricSettings(m, i);
          break;
        }
      }
    }
  } else {
    metricData.count = 0;
    for (JsonObject metricObj : metricsArray) {
      if (metricData.count >= MAX_METRICS) break;
      Metric& m = metricData.metrics[metricData.count];
      readMetric(metricObj, m);
      applyMetricSettings(m, metricData.count);
      metricData.count++;
    }
  }

  metricData.online = true;
}

void parseStats(const char* json, uint8_t link) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  parseStatsV2(doc);
  metricData.link = link;
}

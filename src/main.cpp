/*
 * CYD Game Clock - Main Entry Point
 *
 * ESP32-3248S035R (3.5" CYD, ST7796 480x320, XPT2046 touch)
 * Animated game clocks, plus PC stats sent by the Windows daemon over
 * encrypted WiFi or a USB cable. Ported from SmallOLED-PCMonitor.
 */

#include "config/user_config.h"

#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "config/config.h"
#include "config/settings.h"
#include "utils/utils.h"
#include "display/display.h"
#include "clocks/clocks.h"
#include "clocks/clock_globals.h"
#include "metrics/metrics.h"
#include "network/network.h"
#include "network/portal.h"
#include "network/security.h"
#include "ui/ui.h"
#include "web/web.h"

// ========== Global State ==========
Settings settings;
MetricData metricData;
bool displayAvailable = false;
bool ntpSynced = false;
unsigned long lastNtpSyncTime = 0;
unsigned long lastReceived = 0;
unsigned long wifiDisconnectTime = 0;
unsigned long nextDisplayUpdate = 0;
bool wifiConnected = false;  // WiFi connection status for icon display

bool manualClockMode = false;  // Clock chosen with the clock icon while the PC is online
static bool statsPinned = false;  // PC icon pressed: show stats (or "waiting") even when offline
static bool infoOpen = false;
static unsigned long infoOpenedAt = 0;

// Clock styles the clock icon cycles through (4 is the reserved "Space Ship" slot)
static const uint8_t CLOCK_STYLES[] = {0, 1, 2, 3, 5, 6};

// ========== Helper Functions ==========

// Helper function to get time with short timeout
bool getTimeWithTimeout(struct tm *timeinfo, unsigned long timeout_ms) {
  if (!ntpSynced) {
    if (getLocalTime(timeinfo, timeout_ms)) {
      // Verify time is reasonable (year > 2020) before accepting
      if (timeinfo->tm_year > 120) { // tm_year is years since 1900
        ntpSynced = true;
        lastNtpSyncTime = millis();
        Serial.println("NTP successfully synchronized");
        return true;
      }
    }
    return false;
  }
  return getLocalTime(timeinfo, timeout_ms);
}

// Returns optimal refresh rate in Hz based on current display mode
int getOptimalRefreshRate() {
  if (settings.refreshRateMode == 1) {
    // Manual mode - use user-specified rate
    return settings.refreshRateHz;
  }

  if (uiView() == VIEW_CLOCK) {
    bool animatedStyle = settings.clockStyle == 0 || settings.clockStyle == 3 ||
                         settings.clockStyle == 4 || settings.clockStyle == 5 ||
                         settings.clockStyle == 6;

    // Animation is happening - boost to 40 Hz for smooth motion
    if (settings.boostAnimationRefresh && animatedStyle && (manualClockMode || isAnimationActive())) {
      return 40;
    }
    // Animated clocks: 20 Hz keeps character movement smooth;
    // static clocks only change once a second
    return animatedStyle ? 20 : 2;
  }
  return 10;  // PC stats (updated every few seconds by the daemon)
}

static UiView wantedView() {
  if (infoOpen) return VIEW_INFO;
  if (statsPinned || (metricData.online && !manualClockMode)) return VIEW_STATS;
  return VIEW_CLOCK;
}

static void nextClockStyle() {
  int idx = 0;
  for (size_t i = 0; i < sizeof(CLOCK_STYLES); i++) {
    if (CLOCK_STYLES[i] == settings.clockStyle) idx = i;
  }
  settings.clockStyle = CLOCK_STYLES[(idx + 1) % sizeof(CLOCK_STYLES)];
  Serial.printf("Clock style -> %d\n", settings.clockStyle);
  saveSettings();
}

static void handleTouch() {
  switch (uiPollTouch()) {
    case UI_EVENT_CLOCK_ICON:
      // First tap shows the clock; further taps cycle the clock styles
      if (wantedView() == VIEW_CLOCK) {
        nextClockStyle();
      }
      infoOpen = false;
      statsPinned = false;
      manualClockMode = metricData.online;
      break;
    case UI_EVENT_PC_ICON:
      infoOpen = false;
      statsPinned = true;
      manualClockMode = false;
      break;
    case UI_EVENT_INFO_ICON:
      infoOpen = !infoOpen;
      infoOpenedAt = millis();
      break;
    case UI_EVENT_LONG_PRESS:
#if LED_PWM_ENABLED
      enableLED(!settings.ledEnabled);
      saveSettings();
#endif
      break;
    case UI_EVENT_FORGET_WIFI:
      Serial.println("Forget WiFi requested on the info screen");
      uiShowMessage("WiFi settings cleared", "Restarting into WiFi setup...", nullptr);
      wifiCredsClear();
      setWebPassword("");  // physical access also recovers a forgotten website password
      delay(1500);
      ESP.restart();
      break;
    default:
      break;
  }
  if (infoOpen && millis() - infoOpenedAt > INFO_SCREEN_TIMEOUT) infoOpen = false;
}

static void renderCanvas(UiView view) {
  // Mario plays against the Super Mario Bros. sky; everything else on black
  bool marioSky = view == VIEW_CLOCK && settings.clockStyle == 0;
  display.setBackground(GameCanvas::paletteColour(marioSky ? COL_SKY : COL_INK));
  display.clearDisplay();
  display.setTextColor(DISPLAY_WHITE);
  display.setTextSize(1);

  if (view == VIEW_STATS) {
    if (!metricData.online) {
      displayWaitingForPC();
    } else if (metricData.status != STATUS_OK && metricData.status != 0) {
      // PC is connected but LibreHardwareMonitor has issues
      displayErrorStatus(metricData.status);
    } else {
      displayStats();
    }
  } else {
    switch (settings.clockStyle) {
      case 0: displayClockWithMario(); break;
      case 1: displayStandardClock(); break;
      case 2: displayLargeClock(); break;
      case 3:
      case 4: displayClockWithSpaceInvader(); break;
      case 5: displayClockWithPong(); break;
      case 6: displayClockWithPacman(); break;
      default: displayStandardClock(); break;
    }
  }
  display.pushToTft();
}

// ========== setup() ==========
void setup() {
  Serial.begin(115200);
  Serial.setRxBufferSize(4096);
  delay(200);
  Serial.println("\n[main] " DEVICE_NAME " v" FIRMWARE_VERSION);

  // Load settings from flash
  loadSettings();
  securityBegin();

  displayAvailable = initDisplay();
  applyDisplayBrightness();
  display.setOrigin(UI_CANVAS_X, UI_CANVAS_Y);

#if LED_PWM_ENABLED
  initLEDPWM();
  setLEDBrightness(settings.ledBrightness);
#endif

  // Configure hardware watchdog timer
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL);

  String ssid, pass;
  if (!wifiCredsLoad(ssid, pass)) {
    Serial.println("[main] no WiFi credentials - starting setup portal");
    runProvisioningPortal();  // restarts when done
  }

  esp_task_wdt_delete(NULL);  // connecting can take longer than the watchdog
  wifiConnected = connectWiFi(ssid, pass);
  if (wifiConnected) {
    startNetworkServices();
    initNTP();
  } else {
    applyTimezone();
  }
  esp_task_wdt_add(NULL);

  metricData.count = 0;
  metricData.online = false;
  metricData.status = 0;  // No status received yet
  metricData.link = LINK_NONE;
  metricData.rejected = 0;

  setupWebServer();

  if (wifiConnected) {
    uiShowConnected();
    unsigned long until = millis() + 6000;
    while (millis() < until) {
      esp_task_wdt_reset();
      server.handleClient();
      if (uiPollTouch() != UI_EVENT_NONE) break;  // tap to skip
      delay(10);
    }
  }
  uiSetView(wantedView());
  display.invalidate();
}

// ========== loop() ==========
void loop() {
  esp_task_wdt_reset();

  checkScheduledBrightness();
  server.handleClient();

  // Always process both links to track PC online status accurately
  handleUDP();
  handleSerial();
  handleTouch();

  // Check timeout
  if (millis() - lastReceived > TIMEOUT && metricData.online) {
    metricData.online = false;
    metricData.link = LINK_NONE;
    // Reset manual clock mode so PC metrics auto-show when PC comes back online
    manualClockMode = false;
    Serial.println("PC stats timeout - switching to clock mode");
  }

  // Retry NTP sync periodically if not synced
  if (!ntpSynced && millis() - lastNtpSyncTime > 30000) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0) && timeinfo.tm_year > 120) {
      ntpSynced = true;
      Serial.println("NTP sync successful (retry)");
    }
    lastNtpSyncTime = millis();
  }

  UiView view = wantedView();
  if (view != uiView()) {
    uiSetView(view);
    display.invalidate();
    nextDisplayUpdate = 0;
  }

  if (displayAvailable && millis() >= nextDisplayUpdate) {
    nextDisplayUpdate = millis() + 1000 / max(1, getOptimalRefreshRate());
    uiDrawTopBar();
    if (view == VIEW_INFO) {
      uiDrawInfo();
    } else {
      renderCanvas(view);
    }
  }

  // WiFi reconnection handling
  handleWiFiReconnection();
}

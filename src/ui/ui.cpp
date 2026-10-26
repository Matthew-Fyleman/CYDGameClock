/*
 * CYD Game Clock - Native screen UI
 */

#include "ui.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <qrcode.h>
#include <time.h>
#include "../config/config.h"
#include "../display/tft.h"
#include "../network/security.h"

extern TFT_eSPI tft;

// ========== Layout ==========
static const int ICON_SIZE = 32;
static const int ICON_GAP = 5;    // spacing between the icons
static const int ICON_MARGIN = 8; // from the right edge of the screen
static const int ICON_Y = (UI_TOP_BAR_H - ICON_SIZE) / 2;
static const int ICON_INFO_X = TFT_SCREEN_W - ICON_MARGIN - ICON_SIZE;
static const int ICON_PC_X = ICON_INFO_X - ICON_GAP - ICON_SIZE;
static const int ICON_CLOCK_X = ICON_PC_X - ICON_GAP - ICON_SIZE;

static const int CANVAS_W = SCREEN_WIDTH * CANVAS_SCALE;
static const int CANVAS_H = SCREEN_HEIGHT * CANVAS_SCALE;

// "Forget WiFi" button on the info screen
static const int FORGET_X = 330, FORGET_Y = 52, FORGET_W = 140, FORGET_H = 32;
static const unsigned long FORGET_HOLD_MS = 3000;

static const uint16_t COL_BAR_BG = 0x10A2;    // top bar
static const uint16_t COL_FRAME = 0x4208;     // bezel around the game screen
static const uint16_t COL_HILITE = 0x2A69;    // active icon background
static const uint16_t COL_LABEL = 0x9CF3;

static UiView currentView = VIEW_CLOCK;
static bool topBarDirty = true;
static bool infoDirty = true;

// ========== Generic screens ==========
static void clearBelowBar() { tft.fillRect(0, UI_TOP_BAR_H, TFT_SCREEN_W, TFT_SCREEN_H - UI_TOP_BAR_H, TFT_BLACK); }

void uiShowMessage(const char* title, const char* line1, const char* line2) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(title, TFT_SCREEN_W / 2, 120, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (line1) tft.drawString(line1, TFT_SCREEN_W / 2, 170, 2);
  if (line2) tft.drawString(line2, TFT_SCREEN_W / 2, 196, 2);
  tft.setTextDatum(TL_DATUM);
  topBarDirty = true;
}

void uiShowConnecting(const char* ssid, int seconds, int timeoutSeconds) {
  static bool drawn = false;
  if (!drawn || seconds == 0) {
    uiShowMessage("Connecting to WiFi", ssid, nullptr);
    drawn = true;
  }
  // Progress bar
  int w = 300, x = (TFT_SCREEN_W - w) / 2, y = 220;
  tft.drawRect(x, y, w, 14, TFT_DARKGREY);
  int fill = map(min(seconds, timeoutSeconds), 0, timeoutSeconds, 0, w - 4);
  tft.fillRect(x + 2, y + 2, fill, 10, TFT_CYAN);
}

void uiShowConnected() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Connected!", TFT_SCREEN_W / 2, 60, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(WiFi.localIP().toString(), TFT_SCREEN_W / 2, 100, 4);
  tft.setTextColor(COL_LABEL, TFT_BLACK);
  tft.drawString("Settings: http://" MDNS_HOSTNAME ".local", TFT_SCREEN_W / 2, 140, 2);
  tft.drawString("(or http://" + WiFi.localIP().toString() + ")", TFT_SCREEN_W / 2, 160, 2);
  tft.drawString("Pairing code for the PC daemon:", TFT_SCREEN_W / 2, 205, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(pairingCodeFormatted(), TFT_SCREEN_W / 2, 235, 4);
  tft.setTextColor(COL_LABEL, TFT_BLACK);
  tft.drawString("Tap the (i) icon at any time to see this again", TFT_SCREEN_W / 2, 280, 2);
  tft.setTextDatum(TL_DATUM);
  topBarDirty = true;
}

void uiShowPortal(const char* apName, const char* apPass, const char* apIp) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("WiFi setup", 12, 10, 4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int y = 52;
  tft.drawString("1. On your phone, scan the code", 12, y, 2); y += 18;
  tft.drawString("   or join this WiFi network:", 12, y, 2); y += 24;
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(apName, 24, y, 4); y += 30;
  tft.setTextColor(COL_LABEL, TFT_BLACK);
  tft.drawString("Password:", 24, y + 6, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(apPass, 104, y, 4); y += 38;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("2. The setup page opens by itself", 12, y, 2); y += 18;
  tft.drawString(String("   (or browse to http://") + apIp + ")", 12, y, 2); y += 24;
  tft.drawString("3. Pick your home network and", 12, y, 2); y += 18;
  tft.drawString("   enter its password", 12, y, 2);

  // QR code that joins the setup network
  String payload = String("WIFI:T:WPA;S:") + apName + ";P:" + apPass + ";;";
  QRCode qr;
  uint8_t qrData[qrcode_getBufferSize(4)];
  qrcode_initText(&qr, qrData, 4, ECC_LOW, payload.c_str());
  const int scale = 5;
  int size = qr.size * scale;
  int qx = TFT_SCREEN_W - size - 20, qy = 60;
  tft.fillRect(qx - 10, qy - 10, size + 20, size + 20, TFT_WHITE);
  for (int my = 0; my < qr.size; my++) {
    for (int mx = 0; mx < qr.size; mx++) {
      if (qrcode_getModule(&qr, mx, my)) tft.fillRect(qx + mx * scale, qy + my * scale, scale, scale, TFT_BLACK);
    }
  }
  uiShowPortalStatus("Waiting for you to choose a network", TFT_DARKGREY);
}

void uiShowPortalStatus(const char* text, uint16_t colour) {
  tft.fillRect(0, TFT_SCREEN_H - 34, TFT_SCREEN_W, 34, TFT_BLACK);
  tft.setTextColor(colour == TFT_DARKGREY ? COL_LABEL : colour, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(text, TFT_SCREEN_W / 2, TFT_SCREEN_H - 17, 2);
  tft.setTextDatum(TL_DATUM);
}

// ========== Icons ==========
static void drawClockIcon(int x, int y, uint16_t fg) {
  int cx = x + ICON_SIZE / 2, cy = y + ICON_SIZE / 2;
  tft.drawCircle(cx, cy, 13, fg);
  tft.drawCircle(cx, cy, 12, fg);
  for (int i = 0; i < 12; i += 3) {
    float a = i * PI / 6;
    tft.drawPixel(cx + (int)(10 * sin(a)), cy - (int)(10 * cos(a)), fg);
  }
  tft.drawLine(cx, cy, cx, cy - 8, fg);       // minute hand
  tft.drawLine(cx, cy, cx + 6, cy + 2, fg);   // hour hand
  tft.fillCircle(cx, cy, 1, fg);
}

static void drawPcIcon(int x, int y, uint16_t fg) {
  tft.drawRoundRect(x + 3, y + 5, 26, 18, 2, fg);  // monitor
  tft.fillRect(x + 6, y + 8, 20, 12, 0x0230);      // screen
  tft.drawFastHLine(x + 8, y + 12, 8, fg);         // "text" on the screen
  tft.drawFastHLine(x + 8, y + 16, 12, fg);
  tft.fillRect(x + 14, y + 23, 4, 4, fg);          // stand
  tft.fillRect(x + 9, y + 27, 14, 2, fg);          // base
}

static void drawInfoIcon(int x, int y, uint16_t fg) {
  int cx = x + ICON_SIZE / 2, cy = y + ICON_SIZE / 2;
  tft.fillCircle(cx, cy, 13, 0x2B5F);
  tft.drawCircle(cx, cy, 13, fg);
  tft.fillCircle(cx, cy - 7, 2, fg);
  tft.fillRect(cx - 2, cy - 3, 4, 11, fg);
  tft.fillRect(cx - 4, cy + 7, 8, 2, fg);
}

static void drawIcon(int x, UiView view, void (*draw)(int, int, uint16_t)) {
  bool active = currentView == view;
  tft.fillRoundRect(x - 2, ICON_Y - 2, ICON_SIZE + 4, ICON_SIZE + 4, 5, active ? COL_HILITE : COL_BAR_BG);
  draw(x, ICON_Y, active ? TFT_CYAN : TFT_WHITE);
}

// ========== Top bar ==========
static void formatDate(char* out, size_t len) {
  struct tm t;
  if (!ntpSynced || !getLocalTime(&t, 0)) {
    snprintf(out, len, "Waiting for time...");
    return;
  }
  static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  switch (settings.dateFormat) {
    case 1:
      snprintf(out, len, "%s %02d/%02d/%04d", days[t.tm_wday], t.tm_mon + 1, t.tm_mday, t.tm_year + 1900);
      break;
    case 2:
      snprintf(out, len, "%s %04d-%02d-%02d", days[t.tm_wday], t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
      break;
    default:
      snprintf(out, len, "%s %02d/%02d/%04d", days[t.tm_wday], t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
      break;
  }
}

void uiDrawTopBar() {
  static char shownDate[32] = "";
  char date[32];
  formatDate(date, sizeof(date));

  if (topBarDirty) {
    tft.fillRect(0, 0, TFT_SCREEN_W, UI_TOP_BAR_H, COL_BAR_BG);
    tft.drawFastHLine(0, UI_TOP_BAR_H - 1, TFT_SCREEN_W, COL_FRAME);
    drawIcon(ICON_CLOCK_X, VIEW_CLOCK, drawClockIcon);
    drawIcon(ICON_PC_X, VIEW_STATS, drawPcIcon);
    drawIcon(ICON_INFO_X, VIEW_INFO, drawInfoIcon);
    shownDate[0] = '\0';
    topBarDirty = false;
  }

  if (strcmp(date, shownDate) != 0) {
    strcpy(shownDate, date);
    tft.fillRect(0, 0, ICON_CLOCK_X - 4, UI_TOP_BAR_H - 1, COL_BAR_BG);
    tft.setTextColor(TFT_WHITE, COL_BAR_BG);
    tft.drawString(date, 12, (UI_TOP_BAR_H - 26) / 2 + 1, 4);
  }
}

// ========== Views ==========
static void drawGameFrame() {
  clearBelowBar();
  tft.drawRoundRect(UI_CANVAS_X - 6, UI_CANVAS_Y - 6, CANVAS_W + 12, CANVAS_H + 12, 8, COL_FRAME);
  tft.drawRoundRect(UI_CANVAS_X - 5, UI_CANVAS_Y - 5, CANVAS_W + 10, CANVAS_H + 10, 7, COL_FRAME);
}

void uiSetView(UiView view) {
  currentView = view;
  topBarDirty = true;
  if (view == VIEW_INFO) {
    clearBelowBar();
    infoDirty = true;
  } else {
    drawGameFrame();
  }
}

UiView uiView() { return currentView; }

static void infoRow(int y, const char* label, const String& value, uint16_t colour = TFT_WHITE) {
  tft.setTextColor(COL_LABEL, TFT_BLACK);
  tft.drawString(label, 12, y, 2);
  tft.fillRect(150, y, TFT_SCREEN_W - 150, 16, TFT_BLACK);
  tft.setTextColor(colour, TFT_BLACK);
  tft.drawString(value, 150, y, 2);
}

static String formatDuration(unsigned long secs) {
  char buf[32];
  if (secs >= 86400) {
    snprintf(buf, sizeof(buf), "%lud %luh %lum", secs / 86400, (secs / 3600) % 24, (secs / 60) % 60);
  } else if (secs >= 3600) {
    snprintf(buf, sizeof(buf), "%luh %lum", secs / 3600, (secs / 60) % 60);
  } else if (secs >= 60) {
    snprintf(buf, sizeof(buf), "%lum %lus", secs / 60, secs % 60);
  } else {
    snprintf(buf, sizeof(buf), "%lus", secs);
  }
  return buf;
}

static void drawForgetButton(float progress) {
  tft.fillRoundRect(FORGET_X, FORGET_Y, FORGET_W, FORGET_H, 6, 0x6000);
  if (progress > 0) {
    tft.fillRoundRect(FORGET_X, FORGET_Y, (int)(FORGET_W * progress), FORGET_H, 6, TFT_RED);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(progress > 0 ? "Keep holding..." : "Forget WiFi", FORGET_X + FORGET_W / 2,
                 FORGET_Y + FORGET_H / 2, 2);
  tft.setTextDatum(TL_DATUM);
}

void uiDrawInfo() {
  static unsigned long lastDraw = 0;
  if (!infoDirty && millis() - lastDraw < 1000) return;
  lastDraw = millis();

  if (infoDirty) {
    infoDirty = false;
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(DEVICE_NAME, 12, 54, 4);
    tft.setTextColor(COL_LABEL, TFT_BLACK);
    tft.drawString("v" FIRMWARE_VERSION, 218, 62, 2);
    drawForgetButton(0);
  }

  int y = 96;
  const int step = 21;
  infoRow(y, "Pairing code", pairingCodeFormatted(), TFT_YELLOW); y += step;
  if (WiFi.status() == WL_CONNECTED) {
    infoRow(y, "WiFi network", WiFi.SSID() + "  (" + String(WiFi.RSSI()) + " dBm)"); y += step;
    infoRow(y, "IP address", WiFi.localIP().toString()); y += step;
  } else {
    infoRow(y, "WiFi network", "Not connected - retrying", TFT_ORANGE); y += step;
    infoRow(y, "IP address", "-"); y += step;
  }
  infoRow(y, "Settings page", "http://" MDNS_HOSTNAME ".local"); y += step;
  infoRow(y, "Settings login", webPasswordIsDefault() ? "admin / the pairing code" : "admin / your password"); y += step;

  String link;
  uint16_t linkColour = TFT_WHITE;
  if (!metricData.online) {
    link = "No PC connected";
    linkColour = TFT_ORANGE;
  } else if (metricData.link == LINK_WIFI) {
    link = "WiFi, encrypted (AES-256)";
    linkColour = TFT_GREEN;
  } else {
    link = "USB serial cable";
    linkColour = TFT_GREEN;
  }
  infoRow(y, "PC stats link", link, linkColour); y += step;
  infoRow(y, "Last PC data", lastReceived ? formatDuration((millis() - lastReceived) / 1000) + " ago" : "never"); y += step;
  infoRow(y, "Rejected packets", String(metricData.rejected), metricData.rejected ? TFT_ORANGE : TFT_WHITE); y += step;
  infoRow(y, "MAC address", WiFi.macAddress()); y += step;
  infoRow(y, "Uptime", formatDuration(millis() / 1000)); y += step;
  infoRow(y, "Free memory", String(ESP.getFreeHeap() / 1024) + " KB");
}

// ========== Touch ==========
UiEvent uiPollTouch() {
  static bool wasDown = false;
  static unsigned long downAt = 0;
  static int downX = 0, downY = 0;
  static bool longFired = false;
  static unsigned long lastIconTap = 0;

  static unsigned long lastSeen = 0;

  int x = downX, y = downY;
  unsigned long now = millis();
  // Resistive panels drop out for a moment during a press: only count it as
  // released after 80 ms without a reading
  bool down = touchRead(x, y);
  if (down) {
    lastSeen = now;
  } else if (wasDown && now - lastSeen < 80) {
    down = true;
  }
  UiEvent ev = UI_EVENT_NONE;

  if (down && !wasDown) {
    downAt = now;
    downX = x;
    downY = y;
    longFired = false;

    // Icons react as soon as they are touched
    if (y < UI_TOP_BAR_H + 6 && now - lastIconTap > 300) {
      if (x >= ICON_INFO_X - 2) {
        ev = UI_EVENT_INFO_ICON;
      } else if (x >= ICON_PC_X - 2) {
        ev = UI_EVENT_PC_ICON;
      } else if (x >= ICON_CLOCK_X - 2) {
        ev = UI_EVENT_CLOCK_ICON;
      }
      if (ev != UI_EVENT_NONE) {
        lastIconTap = now;
        longFired = true;  // no long press for icon touches
      }
    }
  } else if (down && !longFired) {
    bool onForget = currentView == VIEW_INFO && downX >= FORGET_X && downX < FORGET_X + FORGET_W &&
                    downY >= FORGET_Y && downY < FORGET_Y + FORGET_H;
    if (onForget) {
      float progress = (float)(now - downAt) / FORGET_HOLD_MS;
      static unsigned long lastBar = 0;
      if (now - lastBar > 100) {
        lastBar = now;
        drawForgetButton(min(progress, 1.0f));
      }
      if (progress >= 1.0f) {
        longFired = true;
        ev = UI_EVENT_FORGET_WIFI;
      }
    } else if (currentView != VIEW_INFO && downY > UI_TOP_BAR_H && now - downAt >= TOUCH_LONG_PRESS_MS) {
      longFired = true;
      ev = UI_EVENT_LONG_PRESS;
    }
  } else if (!down && wasDown && currentView == VIEW_INFO && !longFired) {
    drawForgetButton(0);  // let go too early
  }

  wasDown = down;
  return ev;
}

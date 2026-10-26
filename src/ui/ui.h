/*
 * CYD Game Clock - Native screen UI
 *
 * Everything drawn at full TFT resolution rather than on the scaled game
 * canvas: the top bar (date and the three touch icons), the information
 * screen and the start-up / WiFi setup screens.
 */

#ifndef UI_H
#define UI_H

#include <Arduino.h>

enum UiView { VIEW_CLOCK, VIEW_STATS, VIEW_INFO };

enum UiEvent {
  UI_EVENT_NONE,
  UI_EVENT_CLOCK_ICON,   // cycle clock styles
  UI_EVENT_PC_ICON,      // show PC stats
  UI_EVENT_INFO_ICON,    // show / hide the information screen
  UI_EVENT_LONG_PRESS,   // held on the clock: toggle night light
  UI_EVENT_FORGET_WIFI,  // held the button on the info screen
};

// Top bar height and where the scaled canvas sits
#define UI_TOP_BAR_H 44
#define UI_CANVAS_X 48
#define UI_CANVAS_Y 86

// Start-up and setup screens
void uiShowMessage(const char* title, const char* line1, const char* line2);
void uiShowConnecting(const char* ssid, int seconds, int timeoutSeconds);
void uiShowConnected();
void uiShowPortal(const char* apName, const char* apPass, const char* apIp);
void uiShowPortalStatus(const char* text, uint16_t colour);

// Main screen: call uiSetView() when the view changes (redraws the frame),
// uiDrawTopBar() every frame (it only redraws what changed)
void uiSetView(UiView view);
UiView uiView();
void uiDrawTopBar();
void uiDrawInfo();

// Poll the touch panel; returns at most one event per call
UiEvent uiPollTouch();

#endif // UI_H

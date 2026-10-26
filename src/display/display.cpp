/*
 * CYD Game Clock - Display Module
 *
 * Display initialization and brightness control (backlight PWM).
 */

#include "display.h"
#include "tft.h"
#include "../config/config.h"
#include <time.h>

GameCanvas display(SCREEN_WIDTH, SCREEN_HEIGHT);

// Track last applied brightness to avoid unnecessary updates
static int lastAppliedBrightness = -1;
static unsigned long lastBrightnessCheck = 0;
const unsigned long BRIGHTNESS_CHECK_INTERVAL = 60000; // Check every minute

// Initialize display - returns true on success
bool initDisplay() {
  tftInit(settings.flipScreen);
  return display.begin();
}

// Apply display brightness from settings
void applyDisplayBrightness() {
  tftSetBacklight(settings.displayBrightness);
  lastAppliedBrightness = settings.displayBrightness;
}

void resetBrightnessSchedule() {
  lastBrightnessCheck = 0;
  lastAppliedBrightness = -1;
}

// Check and apply time-based brightness (scheduled dimming)
void checkScheduledBrightness() {
  // Only check every minute to avoid unnecessary updates
  unsigned long currentTime = millis();
  if (lastBrightnessCheck != 0 && currentTime - lastBrightnessCheck < BRIGHTNESS_CHECK_INTERVAL) {
    return;
  }
  lastBrightnessCheck = currentTime;

  // If scheduled dimming is disabled, ensure normal brightness is applied
  if (!settings.enableScheduledDimming) {
    if (lastAppliedBrightness != settings.displayBrightness) {
      applyDisplayBrightness();
    }
    return;
  }

  // Get current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return; // Can't get time, skip this check
  }

  uint8_t currentHour = timeinfo.tm_hour;
  uint8_t targetBrightness;

  // Check if current time is within dim period
  // Handle wrap-around case (e.g., 22:00 to 07:00)
  if (settings.dimStartHour < settings.dimEndHour) {
    // Normal case: start and end are in same day
    if (currentHour >= settings.dimStartHour && currentHour < settings.dimEndHour) {
      targetBrightness = settings.dimBrightness;
    } else {
      targetBrightness = settings.displayBrightness;
    }
  } else {
    // Wrap-around case: spans midnight
    if (currentHour >= settings.dimStartHour || currentHour < settings.dimEndHour) {
      targetBrightness = settings.dimBrightness;
    } else {
      targetBrightness = settings.displayBrightness;
    }
  }

  // Apply brightness only if it changed
  if (lastAppliedBrightness != targetBrightness) {
    tftSetBacklight(targetBrightness);
    lastAppliedBrightness = targetBrightness;
  }
}

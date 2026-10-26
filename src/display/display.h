/*
 * CYD Game Clock - Display Module
 *
 * The global `display` object is a 128x64 colour canvas with the same drawing
 * API as the OLED it replaces (see canvas.h); brightness drives the backlight.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include "canvas.h"
#include "../config/user_config.h"

extern GameCanvas display;

#define DISPLAY_WHITE COL_WHITE
#define DISPLAY_BLACK COL_BLACK

// Initialize display - returns true on success
bool initDisplay();
void applyDisplayBrightness();
void checkScheduledBrightness();
// Force the brightness check to run on the next loop (after settings change)
void resetBrightnessSchedule();

#endif // DISPLAY_H

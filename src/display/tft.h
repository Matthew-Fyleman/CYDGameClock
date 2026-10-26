/*
 * CYD Game Clock - TFT and touch driver glue
 *
 * Kept free of TFT_eSPI types so it can be included next to Adafruit_GFX.
 */

#ifndef TFT_H
#define TFT_H

#include <Arduino.h>

void tftInit(bool flipped);
void tftSetRotationFlipped(bool flipped);
void tftSetBacklight(uint8_t level);  // 0-255

void tftStartWrite();
void tftEndWrite();
void tftPushBlock(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data);

// Returns true while the panel is touched, with screen coordinates
bool touchRead(int& x, int& y);

#endif // TFT_H

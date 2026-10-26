/*
 * CYD Game Clock - TFT and touch driver glue
 */

#include "tft.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "../config/user_config.h"

TFT_eSPI tft;
static XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
static bool rotationFlipped = false;

void tftInit(bool flipped) {
  // Display and XPT2046 share one SPI bus; keep both chip selects idle first
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, TFT_CS);

  tft.init();
  tft.setSwapBytes(true);
  tftSetRotationFlipped(flipped);
  tft.fillScreen(TFT_BLACK);

  ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQ, 8);
  ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
  tftSetBacklight(255);

  ts.begin(SPI);
  ts.setRotation(0);  // raw orientation; touchRead() maps it to the display rotation
}

void tftSetRotationFlipped(bool flipped) {
  rotationFlipped = flipped;
  tft.setRotation(flipped ? 3 : 1);  // landscape, USB on the left (1) or right (3)
}

void tftSetBacklight(uint8_t level) { ledcWrite(BACKLIGHT_PWM_CHANNEL, level); }

void tftStartWrite() { tft.startWrite(); }
void tftEndWrite() { tft.endWrite(); }

void tftPushBlock(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) {
  tft.pushImage(x, y, w, h, data);
}

bool touchRead(int& x, int& y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  // Calibration is for the panel in portrait; rotation 1 turns it 90 degrees
  int px = constrain(map(p.x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, TFT_SCREEN_H - 1), 0, TFT_SCREEN_H - 1);
  int py = constrain(map(p.y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, TFT_SCREEN_W - 1), 0, TFT_SCREEN_W - 1);
  if (rotationFlipped) {
    x = py;
    y = px;
  } else {
    x = (TFT_SCREEN_W - 1) - py;
    y = (TFT_SCREEN_H - 1) - px;
  }
  return true;
}

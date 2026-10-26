/*
 * CYD Game Clock - Game Canvas
 *
 * A 128x64 Adafruit_GFX canvas holding one palette index per pixel. The clock
 * and stats code draws on it exactly as it drew on the OLED, but in colour;
 * pushToTft() then scales it up on the TFT, sending only the rows that changed.
 */

#ifndef CANVAS_H
#define CANVAS_H

#include <Adafruit_GFX.h>

// Palette indices (colour 0 is the background)
enum : uint8_t {
  COL_BLACK = 0,
  COL_WHITE,
  COL_RED,
  COL_GREEN,
  COL_BLUE,
  COL_YELLOW,
  COL_CYAN,
  COL_MAGENTA,
  COL_ORANGE,
  COL_GREY,
  COL_DARKGREY,
  COL_SKY,       // Mario clock background
  COL_INK,       // "black" that stays visible on a coloured background
  COL_PALETTE_SIZE,
  // Not a real colour: picks a colour per text row from setRowColours()
  COL_ROWS = 0x80
};

class GameCanvas : public Adafruit_GFX {
 public:
  GameCanvas(int16_t w, int16_t h);

  bool begin();

  // Adafruit_GFX overrides
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void fillScreen(uint16_t color) override;

  // OLED-compatible helpers
  void clearDisplay() { fillScreen(COL_BLACK); }
  uint8_t getPixelIndex(int16_t x, int16_t y) const;
  bool isLit(int16_t x, int16_t y) const { return getPixelIndex(x, y) != COL_BLACK; }

  // Background colour for the whole canvas (palette slot 0)
  void setBackground(uint16_t rgb565);

  // COL_ROWS draws each text row (rowHeight pixels tall, starting at originY)
  // in the next colour of `colours`
  void setRowColours(const uint8_t* colours, uint8_t count, int16_t originY, uint8_t rowHeight);

  // Where the scaled canvas sits on the TFT
  void setOrigin(int16_t x, int16_t y) { originX_ = x; originY_ = y; }
  int16_t originX() const { return originX_; }
  int16_t originY() const { return originY_; }

  // Send changed rows to the TFT; force = redraw everything
  void pushToTft(bool force = false);
  // Next push redraws everything (after something else drew over the canvas area)
  void invalidate() { forceFull_ = true; }

  static uint16_t paletteColour(uint8_t index);

 private:
  uint8_t resolve(uint16_t color, int16_t y) const;

  uint8_t* buf_ = nullptr;    // drawn this frame
  uint8_t* shown_ = nullptr;  // last pushed to the TFT
  bool forceFull_ = true;
  int16_t originX_ = 0, originY_ = 0;
  const uint8_t* rowColours_ = nullptr;
  uint8_t rowCount_ = 0, rowHeight_ = 1;
  int16_t rowOriginY_ = 0;
};

#endif // CANVAS_H

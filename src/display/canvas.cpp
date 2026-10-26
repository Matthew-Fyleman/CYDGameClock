/*
 * CYD Game Clock - Game Canvas
 */

#include "canvas.h"
#include "tft.h"
#include "../config/user_config.h"

static uint16_t palette[COL_PALETTE_SIZE] = {
    0x0000,  // COL_BLACK (background, changed by setBackground)
    0xFFFF,  // COL_WHITE
    0xF800,  // COL_RED
    0x07E0,  // COL_GREEN
    0x34DF,  // COL_BLUE (lightened so it shows up on black)
    0xFFE0,  // COL_YELLOW
    0x07FF,  // COL_CYAN
    0xF81F,  // COL_MAGENTA
    0xFD20,  // COL_ORANGE
    0x8410,  // COL_GREY
    0x39E7,  // COL_DARKGREY
    0x5A9F,  // COL_SKY (Super Mario Bros. sky blue)
    0x0000,  // COL_INK
};

GameCanvas::GameCanvas(int16_t w, int16_t h) : Adafruit_GFX(w, h) {}

bool GameCanvas::begin() {
  size_t n = (size_t)WIDTH * HEIGHT;
  buf_ = (uint8_t*)calloc(n, 1);
  shown_ = (uint8_t*)calloc(n, 1);
  forceFull_ = true;
  return buf_ && shown_;
}

uint16_t GameCanvas::paletteColour(uint8_t index) {
  return index < COL_PALETTE_SIZE ? palette[index] : palette[COL_WHITE];
}

void GameCanvas::setBackground(uint16_t rgb565) {
  if (palette[COL_BLACK] != rgb565) {
    palette[COL_BLACK] = rgb565;
    forceFull_ = true;
  }
}

void GameCanvas::setRowColours(const uint8_t* colours, uint8_t count, int16_t originY,
                               uint8_t rowHeight) {
  rowColours_ = colours;
  rowCount_ = count;
  rowOriginY_ = originY;
  rowHeight_ = rowHeight ? rowHeight : 1;
}

uint8_t GameCanvas::resolve(uint16_t color, int16_t y) const {
  if (color != COL_ROWS) return color < COL_PALETTE_SIZE ? color : COL_WHITE;
  if (!rowColours_ || rowCount_ == 0) return COL_WHITE;
  int row = (y - rowOriginY_) / rowHeight_;
  if (row < 0) row = 0;
  return rowColours_[row % rowCount_];
}

uint8_t GameCanvas::getPixelIndex(int16_t x, int16_t y) const {
  if (!buf_ || x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return COL_BLACK;
  return buf_[y * WIDTH + x];
}

void GameCanvas::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (!buf_ || x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
  buf_[y * WIDTH + x] = resolve(color, y);
}

void GameCanvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!buf_ || w <= 0 || h <= 0) return;
  int16_t x1 = x + w, y1 = y + h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x1 > WIDTH) x1 = WIDTH;
  if (y1 > HEIGHT) y1 = HEIGHT;
  for (int16_t yy = y; yy < y1; yy++) {
    uint8_t c = resolve(color, yy);
    if (x < x1) memset(buf_ + yy * WIDTH + x, c, x1 - x);
  }
}

void GameCanvas::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRect(x, y, w, 1, color);
}

void GameCanvas::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRect(x, y, 1, h, color);
}

void GameCanvas::fillScreen(uint16_t color) {
  if (buf_) memset(buf_, resolve(color, 0), (size_t)WIDTH * HEIGHT);
}

void GameCanvas::pushToTft(bool force) {
  if (!buf_) return;
  force = force || forceFull_;
  forceFull_ = false;

  static uint16_t line[SCREEN_WIDTH * CANVAS_SCALE * CANVAS_SCALE];
  tftStartWrite();
  for (int y = 0; y < HEIGHT; y++) {
    uint8_t* src = buf_ + y * WIDTH;
    uint8_t* old = shown_ + y * WIDTH;

    // Find the changed span on this row
    int x0 = 0, x1 = WIDTH - 1;
    if (!force) {
      while (x0 < WIDTH && src[x0] == old[x0]) x0++;
      if (x0 == WIDTH) continue;  // row unchanged
      while (x1 > x0 && src[x1] == old[x1]) x1--;
    }

    int spanPx = (x1 - x0 + 1) * CANVAS_SCALE;
    for (int x = x0; x <= x1; x++) {
      uint16_t c = palette[src[x]];
      for (int s = 0; s < CANVAS_SCALE; s++) line[(x - x0) * CANVAS_SCALE + s] = c;
    }
    // Repeat the scaled row CANVAS_SCALE times to make one block
    for (int r = 1; r < CANVAS_SCALE; r++) {
      memcpy(line + r * spanPx, line, spanPx * sizeof(uint16_t));
    }
    tftPushBlock(originX_ + x0 * CANVAS_SCALE, originY_ + y * CANVAS_SCALE, spanPx, CANVAS_SCALE,
                 line);
    memcpy(old + x0, src + x0, x1 - x0 + 1);
  }
  tftEndWrite();
}

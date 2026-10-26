/*
 * CYD Game Clock - Mario Clock Implementation
 *
 * Mario clock style with animated Mario character that jumps to change digits.
 */

#include "../config/config.h"
#include "../display/display.h"
#include "clocks.h"
#include "clock_constants.h"
#include "clock_globals.h"

// Forward declarations for helper functions used by Mario clock
void drawTimeWithBounce();
void advanceDisplayedTime();
void updateSpecificDigit(int digitIndex, int newValue);

// ========== Draw Time With Bounce Effect ==========
void drawTimeWithBounce() {
  display.setTextSize(3);
  display.setTextColor(COL_YELLOW);

  char digits[5];
  digits[0] = '0' + (displayed_hour / 10);
  digits[1] = '0' + (displayed_hour % 10);
  digits[2] = shouldShowColon() ? ':' : ' ';  // Blinking colon
  digits[3] = '0' + (displayed_min / 10);
  digits[4] = '0' + (displayed_min % 10);

  for (int i = 0; i < 5; i++) {
    int y = TIME_Y + (int)digit_offset_y[i];
    display.setCursor(DIGIT_X[i], y);
    display.print(digits[i]);
  }
}

// ========== Advance Displayed Time ==========
void advanceDisplayedTime() {
  displayed_min++;
  if (displayed_min >= 60) {
    displayed_min = 0;
    displayed_hour++;
    if (displayed_hour >= 24) {
      displayed_hour = 0;
    }
  }
  time_overridden = true;
  time_override_start = millis();
}

// ========== Update Specific Digit ==========
void updateSpecificDigit(int digitIndex, int newValue) {
  // Update the specific digit in displayed_hour or displayed_min
  // digitIndex corresponds to DIGIT_X array: 0=hour tens, 1=hour ones, 3=min tens, 4=min ones
  int hour_tens = displayed_hour / 10;
  int hour_ones = displayed_hour % 10;
  int min_tens = displayed_min / 10;
  int min_ones = displayed_min % 10;

  if (digitIndex == 0) {
    hour_tens = newValue;
    displayed_hour = hour_tens * 10 + hour_ones;
  } else if (digitIndex == 1) {
    hour_ones = newValue;
    displayed_hour = hour_tens * 10 + hour_ones;
  } else if (digitIndex == 3) {
    min_tens = newValue;
    displayed_min = min_tens * 10 + min_ones;
  } else if (digitIndex == 4) {
    min_ones = newValue;
    displayed_min = min_tens * 10 + min_ones;
  }

  time_overridden = true;
  time_override_start = millis();
}

// ========== Display Clock With Mario ==========
void displayClockWithMario() {
  struct tm timeinfo;
  if(!getTimeWithTimeout(&timeinfo)) {
    display.setTextSize(1);
    display.setCursor(20, 28);
    if (!ntpSynced) {
      display.print("Syncing time...");
    } else {
      display.print("Time Error");
    }
    return;
  }

  if (!time_overridden) {
    displayed_hour = timeinfo.tm_hour;
    displayed_min = timeinfo.tm_min;
  }

  // Check if time override should be cleared
  if (time_overridden) {
    bool ntp_matches = (timeinfo.tm_hour == displayed_hour && timeinfo.tm_min == displayed_min);
    bool timeout_expired = (millis() - time_override_start > TIME_OVERRIDE_MAX_MS);

    if (ntp_matches || timeout_expired) {
      time_overridden = false;
      // If timeout expired but NTP doesn't match, force sync to real time
      if (timeout_expired && !ntp_matches) {
        displayed_hour = timeinfo.tm_hour;
        displayed_min = timeinfo.tm_min;
      }
    }
  }

  // The date is shown in the top bar of the CYD screen

  updateDigitBounce();
  drawTimeWithBounce();

  updateMarioAnimation(&timeinfo);

  int mario_draw_y = mario_base_y + (int)mario_jump_y;
  bool isJumping = (mario_state == MARIO_JUMPING);
  drawMario((int)mario_x, mario_draw_y, mario_facing_right, mario_walk_frame, isJumping);

  // Draw no-WiFi icon if disconnected
  if (!wifiConnected) {
    drawNoWiFiIcon(0, 0);
  }
}

// ========== Update Mario Animation ==========
void updateMarioAnimation(struct tm* timeinfo) {
  unsigned long currentMillis = millis();

  if (currentMillis - last_mario_update < MARIO_ANIM_SPEED) {
    return;
  }
  last_mario_update = currentMillis;

  int seconds = timeinfo->tm_sec;
  int current_minute = timeinfo->tm_min;

  if (current_minute != last_minute) {
    last_minute = current_minute;
    animation_triggered = false;
  }

  if (seconds >= MARIO_ANIMATION_TRIGGER_SECOND && !animation_triggered && mario_state == MARIO_IDLE) {
    animation_triggered = true;
    calculateTargetDigits(displayed_hour, displayed_min);
    if (num_targets > 0) {
      current_target_index = 0;
      mario_x = MARIO_START_X;
      mario_state = MARIO_WALKING;
      mario_facing_right = true;
      digit_bounce_triggered = false;
    }
  }

  switch (mario_state) {
    case MARIO_IDLE:
      mario_walk_frame = 0;
      mario_x = MARIO_START_X;
      break;

    case MARIO_WALKING:
      if (current_target_index < num_targets) {
        int target = target_x_positions[current_target_index];

        if (abs(mario_x - target) > MARIO_TARGET_PROXIMITY) {
          float walkSpeed = settings.marioWalkSpeed / 10.0f;
          if (mario_x < target) {
            mario_x += walkSpeed;
            mario_facing_right = true;
          } else {
            mario_x -= walkSpeed;
            mario_facing_right = false;
          }
          int frameCount = settings.marioSmoothAnimation ? 4 : 2;
          mario_walk_frame = (mario_walk_frame + 1) % frameCount;
        } else {
          mario_x = target;
          mario_state = MARIO_JUMPING;
          jump_velocity = JUMP_POWER;
          mario_jump_y = 0;
          digit_bounce_triggered = false;
        }
      } else {
        mario_state = MARIO_WALKING_OFF;
        mario_facing_right = true;
      }
      break;

    case MARIO_JUMPING:
      {
        jump_velocity += GRAVITY;
        mario_jump_y += jump_velocity;

        int mario_head_y = mario_base_y + (int)mario_jump_y - MARIO_HEAD_OFFSET;

        if (!digit_bounce_triggered && mario_head_y <= DIGIT_BOTTOM) {
          digit_bounce_triggered = true;
          triggerDigitBounce(target_digit_index[current_target_index]);

          // Update only the specific digit that Mario just hit
          updateSpecificDigit(target_digit_index[current_target_index],
                             target_digit_values[current_target_index]);

          jump_velocity = MARIO_BOUNCE_VELOCITY;
        }

        if (mario_jump_y >= 0) {
          mario_jump_y = 0;
          jump_velocity = 0;

          current_target_index++;

          if (current_target_index < num_targets) {
            mario_state = MARIO_WALKING;
            mario_facing_right = (target_x_positions[current_target_index] > mario_x);
            digit_bounce_triggered = false;
          } else {
            mario_state = MARIO_WALKING_OFF;
            mario_facing_right = true;
          }
        }
      }
      break;

    case MARIO_WALKING_OFF:
      mario_x += settings.marioWalkSpeed / 10.0f;
      {
        int frameCount = settings.marioSmoothAnimation ? 4 : 2;
        mario_walk_frame = (mario_walk_frame + 1) % frameCount;
      }

      if (mario_x > SCREEN_WIDTH + 15) {
        mario_state = MARIO_IDLE;
        mario_x = MARIO_START_X;
      }
      break;
  }
}

// ========== Draw Mario Sprite ==========
// 8x9 sprite in the original's footprint: black cap, boots and face details,
// white face and hands, red suit. COL_INK is black that stays visible on the
// sky-blue background.
static void marioPixel(int sx, int sy, int px, int py, bool facingRight, uint8_t colour) {
  // Sprite is drawn facing right; mirror the columns when facing left
  display.drawPixel(facingRight ? sx + px : sx + 7 - px, sy + py, colour);
}

static void marioRow(int sx, int sy, int px, int py, int w, bool facingRight, uint8_t colour) {
  for (int i = 0; i < w; i++) marioPixel(sx, sy, px + i, py, facingRight, colour);
}

void drawMario(int x, int y, bool facingRight, int frame, bool jumping) {
  if (x < -10 || x > SCREEN_WIDTH + 10) return;

  int sx = x - 4;
  int sy = y - 10;

  // Cap with its peak towards the front
  marioRow(sx, sy, 2, 0, 4, facingRight, COL_INK);
  marioRow(sx, sy, 1, 1, 6, facingRight, COL_INK);
  // Face: white, with eye and moustache
  marioRow(sx, sy, 2, 2, 4, facingRight, COL_WHITE);
  marioPixel(sx, sy, 4, 2, facingRight, COL_INK);        // eye
  marioRow(sx, sy, 2, 3, 4, facingRight, COL_WHITE);
  marioRow(sx, sy, 4, 3, 2, facingRight, COL_INK);       // moustache
  // Suit
  marioRow(sx, sy, 2, 4, 4, facingRight, COL_RED);
  marioRow(sx, sy, 2, 5, 4, facingRight, COL_RED);

  if (jumping) {
    // Arms up, fist raised to hit the digit
    marioPixel(sx, sy, 1, 4, facingRight, COL_RED);
    marioPixel(sx, sy, 6, 4, facingRight, COL_RED);
    marioPixel(sx, sy, 0, 3, facingRight, COL_WHITE);
    marioPixel(sx, sy, 7, 3, facingRight, COL_WHITE);
    marioRow(sx, sy, 2, 6, 2, facingRight, COL_RED);
    marioRow(sx, sy, 4, 6, 2, facingRight, COL_RED);
    marioRow(sx, sy, 1, 7, 2, facingRight, COL_RED);
    marioRow(sx, sy, 5, 7, 2, facingRight, COL_RED);
    marioRow(sx, sy, 1, 8, 2, facingRight, COL_INK);
    marioRow(sx, sy, 5, 8, 2, facingRight, COL_INK);
    return;
  }

  // Arms (white hands); the front arm swings with the walk
  int swing = frame % 2;
  if (settings.marioSmoothAnimation) {
    marioPixel(sx, sy, 1, 4 - swing, facingRight, COL_WHITE);  // back arm (opposite phase)
  } else {
    marioPixel(sx, sy, 1, 4, facingRight, COL_WHITE);
  }
  marioPixel(sx, sy, 6, 4 + swing, facingRight, COL_WHITE);

  // Legs (red) with black boots; the stride follows the walk frame
  int back, front;
  if (settings.marioSmoothAnimation) {
    static const int BACK[4] = {2, 1, 1, 2};
    static const int FRONT[4] = {4, 4, 5, 5};
    back = BACK[frame % 4];
    front = FRONT[frame % 4];
  } else {
    back = frame == 0 ? 2 : 1;
    front = frame == 0 ? 4 : 5;
  }
  marioRow(sx, sy, back, 6, 2, facingRight, COL_RED);
  marioRow(sx, sy, front, 6, 2, facingRight, COL_RED);
  marioRow(sx, sy, back, 7, 2, facingRight, COL_RED);
  marioRow(sx, sy, front, 7, 2, facingRight, COL_RED);
  marioRow(sx, sy, back, 8, 2, facingRight, COL_INK);
  marioRow(sx, sy, front, 8, 2, facingRight, COL_INK);
}

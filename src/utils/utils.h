/*
 * CYD Game Clock - Utility Functions
 *
 * String manipulation, validation, and helper functions.
 */

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "../config/user_config.h"

// String manipulation
void trimTrailingSpaces(char* str);
void convertCaretToSpaces(char* str);

// Validation helpers
bool validateIP(const char* ip);
bool safeCopyString(char* dest, const char* src, size_t maxLen);
void assertBounds(int value, int minVal, int maxVal, const char* name);

#if LED_PWM_ENABLED
// ========== LED Night Light Functions ==========
void initLEDPWM();
void setLEDBrightness(uint8_t brightness);
void enableLED(bool enable);
#endif

#endif // UTILS_H

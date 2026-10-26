/*
 * CYD Game Clock - User Configuration
 *
 * Hardware and network constants for the ESP32-3248S035R (3.5" CYD).
 * Display SPI pins are set in platformio.ini (TFT_eSPI build flags).
 */

#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_NAME "CYD Game Clock"

// ========== Display Configuration ==========
// Logical canvas the clock and stats screens draw on (same as the original
// 128x64 OLED); it is scaled up CANVAS_SCALE times on the 480x320 panel.
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define CANVAS_SCALE 3

// Physical panel in landscape
#define TFT_SCREEN_W 480
#define TFT_SCREEN_H 320

// Backlight (GPIO 27, set in platformio.ini as TFT_BL)
#define BACKLIGHT_PWM_CHANNEL 7
#define BACKLIGHT_PWM_FREQ 5000

// ========== Touch Configuration (XPT2046) ==========
#define TOUCH_CS_PIN 33
#define TOUCH_IRQ_PIN 36
#define SPI_SCK_PIN 14
#define SPI_MISO_PIN 12
#define SPI_MOSI_PIN 13

// Raw XPT2046 range, calibrated on this board (from the Deskbuddy project)
#define TOUCH_RAW_X_MIN 250
#define TOUCH_RAW_X_MAX 3750
#define TOUCH_RAW_Y_MIN 300
#define TOUCH_RAW_Y_MAX 3880

// Hold anywhere on the clock for this long to toggle the night light
#define TOUCH_LONG_PRESS_MS 1000

// ========== WiFi Configuration ==========
// Setup access point: name gets the last 4 hex digits of the MAC appended,
// and a random WPA2 password is generated and shown on screen.
#define AP_NAME_PREFIX "CYDGameClock-"

// mDNS host name (http://cydgameclock.local)
#define MDNS_HOSTNAME "cydgameclock"

// How long to wait for the router when connecting at boot (ms)
#define WIFI_CONNECT_TIMEOUT 30000

// ========== Network Configuration ==========
// UDP port for receiving PC stats
#define UDP_PORT 4210

// NTP servers
#define NTP_SERVER_PRIMARY "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"

// NTP resync interval (1 hour in ms)
#define NTP_RESYNC_INTERVAL 3600000

// Encrypted packets older or newer than this (seconds) are rejected once
// the clock has the time
#define PACKET_MAX_AGE_S 120

// ========== Timing Configuration ==========
// Timeout for PC stats (ms) - show clock if no data received
#define STATS_TIMEOUT 10000

// Maximum time (ms) that animated clocks can override NTP time
// After this, force resync even if animation is still running
// Prevents clock drift when packets are dropped during animations
#define TIME_OVERRIDE_MAX_MS 60000

// The info screen returns to the previous screen after this long (ms)
#define INFO_SCREEN_TIMEOUT 60000

// ========== Watchdog Configuration ==========
// Watchdog timeout in seconds
#define WATCHDOG_TIMEOUT_SECONDS 30

// ========== Touch Screen ==========
// The touch screen replaces the TTP223 button of the OLED version
#define TOUCH_BUTTON_ENABLED 1

// ========== LED Night Light Configuration ==========
// The CYD's on-board RGB LED (active low) is driven white as a night light
#define LED_PWM_ENABLED 1
#define LED_PIN_R 4
#define LED_PIN_G 16
#define LED_PIN_B 17
#define LED_PWM_CHANNEL_R 4
#define LED_PWM_CHANNEL_G 5
#define LED_PWM_CHANNEL_B 6
#define LED_PWM_FREQ 5000
#define LED_PWM_RESOLUTION 8

#endif // USER_CONFIG_H

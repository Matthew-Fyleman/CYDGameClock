/*
 * CYD Game Clock - Network Module
 *
 * WiFi connection, encrypted UDP and USB-serial stats links, NTP sync.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "../config/config.h"

// ========== Global Network Objects ==========
extern WiFiUDP udp;

// ========== Stored WiFi Credentials (NVS) ==========
bool wifiCredsLoad(String& ssid, String& pass);
void wifiCredsSave(const String& ssid, const String& pass);
void wifiCredsClear();

// ========== Network Functions ==========
// Connect to the stored network (shows progress); false on timeout
bool connectWiFi(const String& ssid, const String& pass);

// Start UDP listener and mDNS once connected
void startNetworkServices();

// Initialize NTP time synchronization
void initNTP();

// Apply timezone settings
void applyTimezone();

// Handle encrypted UDP packets and serial lines from the PC daemon
void handleUDP();
void handleSerial();

// Parse incoming stats JSON
void parseStats(const char* json, uint8_t link);
void parseStatsV2(JsonDocument& doc);

// WiFi reconnection handling
void handleWiFiReconnection();

#endif // NETWORK_H

/*
 * CYD Game Clock - Link Security
 *
 * Stats packets on WiFi are encrypted and authenticated with AES-256-GCM.
 * The key is derived (PBKDF2-HMAC-SHA256) from a pairing code that the clock
 * generates, shows on its info screen, and the PC daemon is given once.
 *
 * Packet: "CGC1" | 12-byte nonce | ciphertext | 16-byte tag  (AAD = "CGC1")
 * Plaintext JSON carries "sid" (session id), "seq" (counter) and "t" (unix
 * time) so recorded packets cannot be replayed.
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>

#define PACKET_MAGIC "CGC1"
#define PACKET_MAGIC_LEN 4
#define PACKET_NONCE_LEN 12
#define PACKET_TAG_LEN 16
#define PACKET_OVERHEAD (PACKET_MAGIC_LEN + PACKET_NONCE_LEN + PACKET_TAG_LEN)

// PBKDF2 parameters - must match the daemon
#define PAIRING_KDF_SALT "CYDGameClock/v1"
#define PAIRING_KDF_ITERATIONS 10000

// Load (or create) the pairing code and web password and derive the key
void securityBegin();

// Pairing code as shown to the user, e.g. "ABCD-EFGH-JKLM-NPQR"
String pairingCodeFormatted();
// Make a new pairing code (the daemon must then be given the new one)
void regeneratePairingCode();

// Settings website login (user name is always "admin")
String webPassword();
bool webPasswordIsDefault();
void setWebPassword(const String& password);  // empty = back to the pairing code

// Decrypt a packet into out (NUL-terminated). Returns plaintext length or -1.
int decryptPacket(const uint8_t* in, size_t len, char* out, size_t outCap);
// Encrypt plaintext into out. Returns packet length or -1.
int encryptPacket(const char* plaintext, size_t len, uint8_t* out, size_t outCap);

// Replay protection: true if (sid, seq, t) is newer than anything accepted
bool acceptPacketCounter(uint32_t sid, uint32_t seq, uint32_t unixTime, bool timeValid);

#endif // SECURITY_H

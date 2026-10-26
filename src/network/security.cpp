/*
 * CYD Game Clock - Link Security
 */

#include "security.h"
#include "../config/user_config.h"
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <time.h>

// Base32 without the easily confused I, O, 0 and 1
static const char CODE_ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
static const int CODE_LEN = 16;  // 80 bits

static char pairingCode[CODE_LEN + 1];
static String customWebPassword;
static uint8_t linkKey[32];
static mbedtls_gcm_context gcm;
static bool gcmReady = false;

static uint32_t lastSid = 0, lastSeq = 0, lastTime = 0;
static bool haveCounter = false;

static void deriveKey() {
  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_pkcs5_pbkdf2_hmac(&md, (const unsigned char*)pairingCode, CODE_LEN,
                            (const unsigned char*)PAIRING_KDF_SALT, strlen(PAIRING_KDF_SALT),
                            PAIRING_KDF_ITERATIONS, sizeof(linkKey), linkKey);
  mbedtls_md_free(&md);

  if (gcmReady) mbedtls_gcm_free(&gcm);
  mbedtls_gcm_init(&gcm);
  gcmReady = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, linkKey, 256) == 0;
  haveCounter = false;
}

static void newCode() {
  for (int i = 0; i < CODE_LEN; i++) pairingCode[i] = CODE_ALPHABET[esp_random() % 32];
  pairingCode[CODE_LEN] = '\0';
}

void securityBegin() {
  Preferences prefs;
  prefs.begin("security", false);
  String code = prefs.getString("pairCode", "");
  if (code.length() == CODE_LEN) {
    strcpy(pairingCode, code.c_str());
  } else {
    newCode();
    prefs.putString("pairCode", pairingCode);
    Serial.println("Security: generated a new pairing code");
  }
  customWebPassword = prefs.getString("webPass", "");
  prefs.end();

  uint32_t t0 = millis();
  deriveKey();
  Serial.printf("Security: link key ready (%lu ms)\n", millis() - t0);
}

String pairingCodeFormatted() {
  String s;
  for (int i = 0; i < CODE_LEN; i++) {
    if (i && i % 4 == 0) s += '-';
    s += pairingCode[i];
  }
  return s;
}

void regeneratePairingCode() {
  newCode();
  Preferences prefs;
  prefs.begin("security", false);
  prefs.putString("pairCode", pairingCode);
  prefs.end();
  deriveKey();
}

String webPassword() {
  return customWebPassword.length() ? customWebPassword : pairingCodeFormatted();
}

bool webPasswordIsDefault() { return customWebPassword.length() == 0; }

void setWebPassword(const String& password) {
  customWebPassword = password;
  Preferences prefs;
  prefs.begin("security", false);
  if (password.length()) {
    prefs.putString("webPass", password);
  } else {
    prefs.remove("webPass");
  }
  prefs.end();
}

int decryptPacket(const uint8_t* in, size_t len, char* out, size_t outCap) {
  if (!gcmReady || len <= PACKET_OVERHEAD) return -1;
  if (memcmp(in, PACKET_MAGIC, PACKET_MAGIC_LEN) != 0) return -1;
  size_t ctLen = len - PACKET_OVERHEAD;
  if (ctLen + 1 > outCap) return -1;

  const uint8_t* nonce = in + PACKET_MAGIC_LEN;
  const uint8_t* ct = nonce + PACKET_NONCE_LEN;
  const uint8_t* tag = ct + ctLen;
  int rc = mbedtls_gcm_auth_decrypt(&gcm, ctLen, nonce, PACKET_NONCE_LEN,
                                    (const uint8_t*)PACKET_MAGIC, PACKET_MAGIC_LEN, tag,
                                    PACKET_TAG_LEN, ct, (uint8_t*)out);
  if (rc != 0) return -1;
  out[ctLen] = '\0';
  return (int)ctLen;
}

int encryptPacket(const char* plaintext, size_t len, uint8_t* out, size_t outCap) {
  if (!gcmReady || len + PACKET_OVERHEAD > outCap) return -1;
  memcpy(out, PACKET_MAGIC, PACKET_MAGIC_LEN);
  uint8_t* nonce = out + PACKET_MAGIC_LEN;
  esp_fill_random(nonce, PACKET_NONCE_LEN);
  uint8_t* ct = nonce + PACKET_NONCE_LEN;
  uint8_t* tag = ct + len;
  int rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, nonce, PACKET_NONCE_LEN,
                                     (const uint8_t*)PACKET_MAGIC, PACKET_MAGIC_LEN,
                                     (const uint8_t*)plaintext, ct, PACKET_TAG_LEN, tag);
  return rc == 0 ? (int)(len + PACKET_OVERHEAD) : -1;
}

bool acceptPacketCounter(uint32_t sid, uint32_t seq, uint32_t unixTime, bool timeValid) {
  if (timeValid) {
    long skew = (long)unixTime - (long)time(nullptr);
    if (skew > PACKET_MAX_AGE_S || skew < -PACKET_MAX_AGE_S) return false;
  }
  if (haveCounter) {
    if (sid == lastSid) {
      if (seq <= lastSeq) return false;
    } else if (unixTime + 5 < lastTime) {
      // A different session must not be older than the one we already have
      return false;
    }
  }
  haveCounter = true;
  lastSid = sid;
  lastSeq = seq;
  if (unixTime > lastTime) lastTime = unixTime;
  return true;
}

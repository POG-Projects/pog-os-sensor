#include "web_auth.h"

#include <Preferences.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>

namespace {
constexpr char kNamespace[] = "pogsensor-auth";
constexpr char kDigestKey[] = "admin_sha256";
constexpr size_t kDigestSize = 32;
constexpr size_t kTokenBytes = 16;
constexpr size_t kTokenChars = kTokenBytes * 2;
constexpr size_t kMaximumTokens = 4;
constexpr int64_t kTokenLifetimeUs = 24LL * 60 * 60 * 1000000;

struct TokenSlot {
  String token;
  int64_t expiresAt = 0;
};

TokenSlot tokens[kMaximumTokens];
bool hasPassword = false;

bool constantTimeEqual(const uint8_t *left, const uint8_t *right,
                       size_t size) {
  volatile uint8_t difference = 0;
  for (size_t index = 0; index < size; ++index) {
    difference |= left[index] ^ right[index];
  }
  return difference == 0;
}

bool hashPassword(const String &password, uint8_t digest[kDigestSize]) {
  return mbedtls_sha256(
             reinterpret_cast<const unsigned char *>(password.c_str()),
             password.length(), digest, 0) == 0;
}
}  // namespace

void webAuthBegin() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    hasPassword = false;
    return;
  }
  hasPassword = preferences.getBytesLength(kDigestKey) == kDigestSize;
  preferences.end();
}

bool webAuthHasPassword() { return hasPassword; }

bool webAuthSetPassword(const String &password) {
  if (password.length() < 8 || password.length() > 128) return false;
  uint8_t digest[kDigestSize];
  if (!hashPassword(password, digest)) return false;
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  bool saved = preferences.putBytes(kDigestKey, digest, sizeof(digest)) ==
               sizeof(digest);
  preferences.end();
  if (saved) hasPassword = true;
  return saved;
}

bool webAuthCheckPassword(const String &password) {
  if (!hasPassword || password.length() > 128) return false;
  uint8_t candidate[kDigestSize];
  uint8_t stored[kDigestSize];
  if (!hashPassword(password, candidate)) return false;
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) return false;
  size_t read = preferences.getBytes(kDigestKey, stored, sizeof(stored));
  preferences.end();
  return read == sizeof(stored) &&
         constantTimeEqual(candidate, stored, sizeof(stored));
}

String webAuthIssueToken() {
  static constexpr char kHex[] = "0123456789abcdef";
  uint8_t random[kTokenBytes];
  esp_fill_random(random, sizeof(random));
  char encoded[kTokenChars + 1];
  for (size_t index = 0; index < sizeof(random); ++index) {
    encoded[index * 2] = kHex[random[index] >> 4];
    encoded[index * 2 + 1] = kHex[random[index] & 0x0f];
  }
  encoded[kTokenChars] = '\0';

  int64_t now = esp_timer_get_time();
  size_t selected = 0;
  for (size_t index = 0; index < kMaximumTokens; ++index) {
    if (!tokens[index].token.length() || tokens[index].expiresAt <= now) {
      selected = index;
      break;
    }
    if (tokens[index].expiresAt < tokens[selected].expiresAt) selected = index;
  }
  tokens[selected].token = encoded;
  tokens[selected].expiresAt = now + kTokenLifetimeUs;
  return String(encoded);
}

bool webAuthTokenValid(const String &token) {
  if (token.length() != kTokenChars) return false;
  int64_t now = esp_timer_get_time();
  for (TokenSlot &slot : tokens) {
    if (slot.expiresAt <= now) {
      slot.token = "";
      continue;
    }
    if (slot.token.length() != kTokenChars) continue;
    if (constantTimeEqual(
            reinterpret_cast<const uint8_t *>(slot.token.c_str()),
            reinterpret_cast<const uint8_t *>(token.c_str()), kTokenChars)) {
      return true;
    }
  }
  return false;
}

bool webAuthRevokeToken(const String &token) {
  if (token.length() != kTokenChars) return false;
  for (TokenSlot &slot : tokens) {
    if (slot.token.length() != kTokenChars) continue;
    if (!constantTimeEqual(
            reinterpret_cast<const uint8_t *>(slot.token.c_str()),
            reinterpret_cast<const uint8_t *>(token.c_str()), kTokenChars)) {
      continue;
    }
    slot.token = "";
    slot.expiresAt = 0;
    return true;
  }
  return false;
}

void webAuthInvalidateTokens() {
  for (TokenSlot &slot : tokens) {
    slot.token = "";
    slot.expiresAt = 0;
  }
}

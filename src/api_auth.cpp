#include "api_auth.h"
#include "config.h"
#include <Preferences.h>
#include <esp_random.h>

static char s_apiKey[33] = {0};  // 32 hex chars + null

// Generate a fresh 16-byte random key, encode as 32 lowercase hex chars.
static void generateKey() {
  uint8_t raw[16];
  esp_fill_random(raw, sizeof(raw));
  for (int i = 0; i < 16; i++) {
    snprintf(&s_apiKey[i * 2], 3, "%02x", raw[i]);
  }
}

void apiAuthInit() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  String stored = prefs.getString(API_KEY_NVS_KEY, "");
  prefs.end();

  if (stored.length() == 32) {
    strlcpy(s_apiKey, stored.c_str(), sizeof(s_apiKey));
    Serial.printf("[auth] API key loaded from NVS: %s****\n", String(s_apiKey).substring(0, 4).c_str());
  } else {
    generateKey();
    Preferences w;
    w.begin(NVS_NAMESPACE, false);
    w.putString(API_KEY_NVS_KEY, s_apiKey);
    w.end();
    Serial.printf("[auth] New API key generated: %s****\n", String(s_apiKey).substring(0, 4).c_str());
  }
}

bool apiAuthCheck(WebServer& server) {
  String provided = server.header("X-API-Key");
  if (provided.length() == 32 && provided == String(s_apiKey)) {
    return true;
  }
  server.sendHeader("Content-Type", "application/json");
  server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  return false;
}

String apiAuthGetKey() {
  return String(s_apiKey);
}

String apiAuthGetKeyHint() {
  // e.g. "A1B2****"
  return String(s_apiKey).substring(0, 4) + "****";
}

void apiAuthRotateKey() {
  generateKey();
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(API_KEY_NVS_KEY, s_apiKey);
  prefs.end();
  Serial.printf("[auth] API key rotated: %s****\n", String(s_apiKey).substring(0, 4).c_str());
}

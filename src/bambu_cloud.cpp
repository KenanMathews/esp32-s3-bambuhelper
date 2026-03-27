#include "bambu_cloud.h"
#include "config.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>

// ---------------------------------------------------------------------------
//  Region-aware URL helpers
// ---------------------------------------------------------------------------
const char* getBambuBroker(CloudRegion region) {
  // Bambu only has US and CN MQTT brokers — EU accounts use the US broker
  switch (region) {
    case REGION_CN: return "cn.mqtt.bambulab.com";
    default:        return "us.mqtt.bambulab.com";
  }
}

const char* getBambuApiBase(CloudRegion region) {
  switch (region) {
    case REGION_CN: return "https://api.bambulab.cn";
    default:        return "https://api.bambulab.com";
  }
}

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

// Set common headers that mimic OrcaSlicer client
static void setSlicerHeaders(HTTPClient& http) {
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "bambu_network_agent/01.09.05.01");
  http.addHeader("X-BBL-Client-Name", "OrcaSlicer");
  http.addHeader("X-BBL-Client-Type", "slicer");
  http.addHeader("X-BBL-Client-Version", "01.09.05.51");
  http.addHeader("X-BBL-Language", "en-US");
  http.addHeader("X-BBL-OS-Type", "linux");
  http.addHeader("X-BBL-OS-Version", "6.2.0");
  http.addHeader("X-BBL-Agent-Version", "01.09.05.01");
  http.addHeader("Accept", "application/json");
}

// Make an HTTPS request.
// Returns HTTP status code, or -1 on error. Response body in `response`.
static int httpsRequest(const char* method, const char* url,
                        const char* body, const char* authToken,
                        String& response) {
  WiFiClientSecure* tls = new (std::nothrow) WiFiClientSecure();
  if (!tls) return -1;
  tls->setInsecure();
  tls->setTimeout(10);

  HTTPClient http;
  if (!http.begin(*tls, url)) {
    delete tls;
    return -1;
  }

  setSlicerHeaders(http);
  if (authToken && strlen(authToken) > 0) {
    String auth = "Bearer ";
    auth += authToken;
    http.addHeader("Authorization", auth);
  }

  int httpCode;
  if (strcmp(method, "GET") == 0) {
    httpCode = http.GET();
  } else {
    httpCode = http.POST(body ? body : "");
  }

  if (httpCode > 0) {
    response = http.getString();
  }

  http.end();
  delete tls;
  return httpCode;
}

// Base64url decode (JWT uses base64url, not standard base64)
static String base64UrlDecode(const char* input, size_t len) {
  // Convert base64url to standard base64
  String b64;
  b64.reserve(len + 4);
  for (size_t i = 0; i < len; i++) {
    char c = input[i];
    if (c == '-') c = '+';
    else if (c == '_') c = '/';
    b64 += c;
  }
  // Add padding
  while (b64.length() % 4 != 0) b64 += '=';

  // Decode
  size_t outLen = 0;
  unsigned char* decoded = nullptr;

  // Use mbedtls base64 decode (available on ESP32)
  mbedtls_base64_decode(nullptr, 0, &outLen, (const unsigned char*)b64.c_str(), b64.length());
  if (outLen == 0) return "";
  decoded = (unsigned char*)malloc(outLen + 1);
  if (!decoded) return "";
  if (mbedtls_base64_decode(decoded, outLen, &outLen, (const unsigned char*)b64.c_str(), b64.length()) != 0) {
    free(decoded);
    return "";
  }
  decoded[outLen] = '\0';
  String result((char*)decoded);
  free(decoded);
  return result;
}

// ---------------------------------------------------------------------------
//  Extract userId from JWT token
// ---------------------------------------------------------------------------
bool cloudExtractUserId(const char* token, char* userId, size_t len) {
  // JWT format: header.payload.signature
  const char* dot1 = strchr(token, '.');
  if (!dot1) return false;
  const char* payloadStart = dot1 + 1;
  const char* dot2 = strchr(payloadStart, '.');
  if (!dot2) return false;

  size_t payloadLen = dot2 - payloadStart;
  String decoded = base64UrlDecode(payloadStart, payloadLen);
  if (decoded.length() == 0) return false;

  JsonDocument doc;
  if (deserializeJson(doc, decoded)) return false;

  // Try common uid field names
  const char* uid = nullptr;
  if (doc["uid"].is<const char*>()) uid = doc["uid"];
  else if (doc["sub"].is<const char*>()) uid = doc["sub"];
  else if (doc["user_id"].is<const char*>()) uid = doc["user_id"];

  if (!uid) return false;

  snprintf(userId, len, "u_%s", uid);
  return true;
}

// ---------------------------------------------------------------------------
//  Fetch userId from profile API (fallback for non-JWT tokens)
// ---------------------------------------------------------------------------
bool cloudFetchUserId(const char* token, char* userId, size_t len, CloudRegion region) {
  String url = String(getBambuApiBase(region)) + "/v1/user-service/my/profile";

  String response;
  int httpCode = httpsRequest("GET", url.c_str(), nullptr, token, response);

  Serial.printf("CLOUD: Profile HTTP %d, len=%d\n", httpCode, response.length());

  if (httpCode != 200) return false;

  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;

  // uid can be a string ("uidStr") or a number ("uid")
  String uidStr;
  if (doc["uidStr"].is<const char*>()) uidStr = (const char*)doc["uidStr"];
  else if (doc["uid"].is<const char*>()) uidStr = (const char*)doc["uid"];
  else if (!doc["uid"].isNull()) uidStr = String((long)doc["uid"].as<long long>());

  if (uidStr.length() == 0) {
    Serial.printf("CLOUD: No uid in profile: %s\n", response.substring(0, 300).c_str());
    return false;
  }

  snprintf(userId, len, "u_%s", uidStr.c_str());
  Serial.printf("CLOUD: Got userId from profile: %s\n", userId);
  return true;
}


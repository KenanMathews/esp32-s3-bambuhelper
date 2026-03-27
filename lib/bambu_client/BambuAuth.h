#pragma once
#include <Arduino.h>
#include "bambu_state.h"
#include "bambu_cloud.h"

// ---------------------------------------------------------------------------
//  BambuAuth — static class wrapping Bambu cloud authentication helpers.
//  Implementation lives in src/bambu_cloud.cpp.
// ---------------------------------------------------------------------------
class BambuAuth {
public:
  // Region-aware MQTT broker hostname (e.g. "us.mqtt.bambulab.com")
  static const char* getBroker(CloudRegion region) {
    return getBambuBroker(region);
  }

  // Region-aware REST API base URL (e.g. "https://api.bambulab.com")
  static const char* getApiBase(CloudRegion region) {
    return getBambuApiBase(region);
  }

  // Extract Bambu user ID from a JWT access token.
  // Writes "u_{uid}" into userId (len bytes).  Returns true on success.
  static bool extractUserId(const char* token, char* userId, size_t len) {
    return cloudExtractUserId(token, userId, len);
  }

  // Fetch user ID from the Bambu profile API (fallback for non-JWT tokens).
  // Writes "u_{uid}" into userId (len bytes).  Returns true on success.
  static bool fetchUserId(const char* token, char* userId, size_t len,
                          CloudRegion region = REGION_US) {
    return cloudFetchUserId(token, userId, len, region);
  }
};

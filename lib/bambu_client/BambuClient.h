#pragma once
#include <Arduino.h>
#include "bambu_state.h"
#include "bambu_mqtt.h"

// ---------------------------------------------------------------------------
//  BambuClient — class facade over the Bambu MQTT connection layer.
//
//  Provides a clean, object-oriented API for the rest of the app.
//  The underlying implementation lives in src/bambu_mqtt.cpp; this class is a
//  thin wrapper that will be fleshed out as the SDK evolves into a standalone
//  library.
//
//  Usage:
//    bambuClient.begin();          // call once after WiFi connects
//    bambuClient.loop();           // call every loop iteration
//    bambuClient.getState(0)       // read printer 0 state
// ---------------------------------------------------------------------------
class BambuClient {
public:
  // ── Lifecycle ─────────────────────────────────────────────────────────────
  void begin()        { initBambuMqtt(); }
  void loop()         { handleBambuMqtt(); }
  void resetBackoff() { resetMqttBackoff(); }

  // ── Printer slot queries ──────────────────────────────────────────────────
  bool    isConfigured(uint8_t slot)  const { return isPrinterConfigured(slot); }
  bool    isAnyConfigured()           const { return isAnyPrinterConfigured(); }
  uint8_t activeCount()               const { return getActiveConnCount(); }

  // ── State access ──────────────────────────────────────────────────────────
  const BambuState& getState(uint8_t slot) const {
    return printers[slot < MAX_ACTIVE_PRINTERS ? slot : 0].state;
  }
  const PrinterConfig& getConfig(uint8_t slot) const {
    return printers[slot < MAX_ACTIVE_PRINTERS ? slot : 0].config;
  }
  const MqttDiag& getDiag(uint8_t slot = 0) const {
    return getMqttDiag(slot);
  }

  // Mutable state — app needs to set flags like finishBuzzerPlayed
  BambuState& mutableState(uint8_t slot) {
    return printers[slot < MAX_ACTIVE_PRINTERS ? slot : 0].state;
  }

  // ── Connection control ────────────────────────────────────────────────────
  void disconnect(uint8_t slot) { disconnectBambuMqtt(slot); }
  void disconnectAll()          { disconnectBambuMqtt(); }

  // ── Debug logging ─────────────────────────────────────────────────────────
  void setDebugLog(bool v) { mqttDebugLog = v; }
};

// Single global instance — include this header to access it.
extern BambuClient bambuClient;

#include "ble_manager.h"
#include "config.h"
#include "settings.h"
#include "bambu_state.h"
#include "api_auth.h"
#include "wifi_manager.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
//  NimBLE server + characteristic handles
// ---------------------------------------------------------------------------
static NimBLEServer*         s_server       = nullptr;
static NimBLECharacteristic* s_charInfo     = nullptr;  // DEVICE_INFO    READ
static NimBLECharacteristic* s_charWifiSt   = nullptr;  // WIFI_STATUS    READ|NOTIFY
static NimBLECharacteristic* s_charWifiCfg  = nullptr;  // WIFI_CONFIG    WRITE
static NimBLECharacteristic* s_charPrinter  = nullptr;  // PRINTER_STATUS READ|NOTIFY

static unsigned long s_lastPrinterNotify = 0;
static bool          s_lastWifiConnected = false;

// ---------------------------------------------------------------------------
//  WIFI_CONFIG write callback — key-gated WiFi credential update
// ---------------------------------------------------------------------------
class WifiConfigCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* chr) override {
    std::string raw = chr->getValue();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, raw.c_str());
    if (err) return;

    // Validate key in payload
    const char* key = doc["key"] | "";
    if (apiAuthGetKey() != String(key)) {
      Serial.println("[ble] WIFI_CONFIG write: bad key");
      return;
    }

    const char* ssid = doc["ssid"] | "";
    const char* pass = doc["pass"] | "";
    if (strlen(ssid) == 0) return;

    strlcpy(wifiSSID, ssid, sizeof(wifiSSID));
    strlcpy(wifiPass, pass, sizeof(wifiPass));
    saveSettings();
    Serial.printf("[ble] WIFI_CONFIG: updating SSID to '%s', restarting in 2s\n", ssid);
    delay(2000);
    ESP.restart();
  }
};

// ---------------------------------------------------------------------------
//  Build JSON for each characteristic
// ---------------------------------------------------------------------------
static String buildDeviceInfo() {
  JsonDocument doc;
  doc["key"]  = apiAuthGetKey();
  doc["host"] = WiFi.localIP().toString();
  doc["fw"]   = FW_VERSION;
  doc["ssid"] = wifiSSID;
  String out;
  serializeJson(doc, out);
  return out;
}

static String buildWifiStatus() {
  JsonDocument doc;
  bool connected = isWiFiConnected() && !isAPMode();
  doc["connected"] = connected;
  doc["ip"]        = connected ? WiFi.localIP().toString() : String("");
  doc["ssid"]      = wifiSSID;
  doc["rssi"]      = WiFi.RSSI();
  String out;
  serializeJson(doc, out);
  return out;
}

static String buildPrinterStatus() {
  BambuState& st = displayedPrinter().state;
  JsonDocument doc;
  doc["state"]          = st.gcodeState;
  doc["progress"]       = st.progress;
  doc["nozzle"]         = (int)st.nozzleTemp;
  doc["bed"]            = (int)st.bedTemp;
  doc["layer"]          = st.layerNum;
  doc["layers"]         = st.totalLayers;
  doc["remaining_mins"] = st.remainingMinutes;
  String out;
  serializeJson(doc, out);
  return out;
}

// ---------------------------------------------------------------------------
//  initBLE
// ---------------------------------------------------------------------------
void initBLE() {
  NimBLEDevice::init(BLE_ADV_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);  // moderate TX power

  s_server = NimBLEDevice::createServer();

  NimBLEService* svc = s_server->createService(BLE_SVC_UUID);

  // DEVICE_INFO — READ only, unauthenticated (proximity is trust anchor)
  s_charInfo = svc->createCharacteristic(
    BLE_CHAR_DEVICE_INFO,
    NIMBLE_PROPERTY::READ
  );

  // WIFI_STATUS — READ + NOTIFY
  s_charWifiSt = svc->createCharacteristic(
    BLE_CHAR_WIFI_STATUS,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  // WIFI_CONFIG — WRITE only (key validated inside callback)
  s_charWifiCfg = svc->createCharacteristic(
    BLE_CHAR_WIFI_CONFIG,
    NIMBLE_PROPERTY::WRITE
  );
  s_charWifiCfg->setCallbacks(new WifiConfigCallback());

  // PRINTER_STATUS — READ + NOTIFY
  s_charPrinter = svc->createCharacteristic(
    BLE_CHAR_PRINTER_STATUS,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  svc->start();

  // Seed initial values
  String info = buildDeviceInfo();
  s_charInfo->setValue(info.c_str());
  String wst = buildWifiStatus();
  s_charWifiSt->setValue(wst.c_str());
  s_lastWifiConnected = isWiFiConnected() && !isAPMode();

  String pst = buildPrinterStatus();
  s_charPrinter->setValue(pst.c_str());

  // Start advertising
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SVC_UUID);
  adv->setScanResponse(true);
  adv->start();

  Serial.printf("[ble] Advertising as '%s'\n", BLE_ADV_NAME);
}

// ---------------------------------------------------------------------------
//  handleBLE — call every loop()
// ---------------------------------------------------------------------------
void handleBLE() {
  if (!s_server) return;

  unsigned long now = millis();

  // WIFI_STATUS notify on change
  bool wifiNow = isWiFiConnected() && !isAPMode();
  if (wifiNow != s_lastWifiConnected) {
    s_lastWifiConnected = wifiNow;
    String wst = buildWifiStatus();
    s_charWifiSt->setValue(wst.c_str());
    s_charWifiSt->notify();
    // Also refresh DEVICE_INFO (IP may have changed)
    String info = buildDeviceInfo();
    s_charInfo->setValue(info.c_str());
  }

  // PRINTER_STATUS notify every 30s
  if (now - s_lastPrinterNotify >= 30000) {
    s_lastPrinterNotify = now;
    String pst = buildPrinterStatus();
    s_charPrinter->setValue(pst.c_str());
    if (s_server->getConnectedCount() > 0) {
      s_charPrinter->notify();
    }
  }
}

bool bleIsConnected() {
  return s_server && s_server->getConnectedCount() > 0;
}

bool bleIsAdvertising() {
  return NimBLEDevice::getAdvertising() && NimBLEDevice::getAdvertising()->isAdvertising();
}

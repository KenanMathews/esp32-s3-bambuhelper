#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void initWiFi();
void handleWiFi();
bool isWiFiConnected();
bool isAPMode();
String getAPSSID();
void forceAPMode();      // switch to AP hotspot immediately (for manual config access)
void reconnectWiFi();    // leave AP mode and reconnect to saved STA credentials

#endif // WIFI_MANAGER_H

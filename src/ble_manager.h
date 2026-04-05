#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

// Start the NimBLE GATT peripheral (call once after WiFi init).
void initBLE();

// Call every loop() — updates PRINTER_STATUS notify and WIFI_STATUS notify.
void handleBLE();

// Returns true if at least one central is connected.
bool bleIsConnected();

// Returns true if currently advertising.
bool bleIsAdvertising();

#endif // BLE_MANAGER_H

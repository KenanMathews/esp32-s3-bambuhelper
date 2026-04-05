#ifndef API_AUTH_H
#define API_AUTH_H

#include <Arduino.h>
#include <WebServer.h>

// Call once inside initWebServer() — loads or generates the API key from NVS.
void   apiAuthInit();

// Call at the top of each protected handler.
// Returns true if key is valid; false if it sent a 401 and the handler must return.
bool   apiAuthCheck(WebServer& server);

// Returns the full 32-char hex key (for QR / BLE characteristic).
String apiAuthGetKey();

// Returns first 4 chars + "****" (safe to expose in /api/mobile/info response).
String apiAuthGetKeyHint();

// Regenerate the key (for "forget device" / rotate flow).
void   apiAuthRotateKey();

#endif // API_AUTH_H

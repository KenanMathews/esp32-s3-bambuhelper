#include <Arduino.h>
#include "display_ui.h"
#include "display_launcher.h"
#include "screen_printer_list.h"
#include "app_manager.h"
#include "lua_runtime.h"
#include "screen_app.h"
#include "lvgl_port.h"
#include "settings.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "config.h"
#include "bambu_state.h"
#include "button.h"
#include "buzzer.h"
#include <BambuClient.h>

// ---------------------------------------------------------------------------
//  App state
// ---------------------------------------------------------------------------
static unsigned long splashEnd         = 0;
static unsigned long finishScreenStart = 0;
static unsigned long idleClockStart    = 0;
static ScreenState   prelaunchScreen   = SCREEN_IDLE;  // screen to restore on launcher dismiss
static char          prevGcodeState[MAX_ACTIVE_PRINTERS][16] = {{0}};

// ---------------------------------------------------------------------------
//  Multi-printer display rotation
// ---------------------------------------------------------------------------
static void handleRotation() {
  if (rotState.mode == ROTATE_OFF) return;
  if (bambuClient.activeCount() < 2) return;

  ScreenState scr = getScreenState();
  if (scr == SCREEN_CLOCK || scr == SCREEN_OFF) {
    bool anyPrinting = false;
    for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
      if (bambuClient.isConfigured(i) && bambuClient.getState(i).printing) {
        anyPrinting = true; break;
      }
    }
    if (!anyPrinting) return;
    setBacklight(brightness);
  }

  unsigned long now = millis();
  if (now - rotState.lastRotateMs < rotState.intervalMs) return;

  uint8_t candidates[MAX_ACTIVE_PRINTERS];
  uint8_t candidateCount = 0;
  uint8_t printingCount  = 0;
  uint8_t printingSlot   = 0xFF;

  for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
    if (!bambuClient.isConfigured(i)) continue;
    if (!bambuClient.getState(i).connected) continue;
    candidates[candidateCount++] = i;
    if (bambuClient.getState(i).printing) { printingCount++; printingSlot = i; }
  }

  if (candidateCount == 0) return;

  if (rotState.mode == ROTATE_SMART && printingCount == 1) {
    if (rotState.displayIndex != printingSlot) {
      rotState.displayIndex = printingSlot;
      triggerDisplayTransition();
    }
    rotState.lastRotateMs = now;
    return;
  }

  uint8_t current = rotState.displayIndex;
  for (uint8_t attempt = 1; attempt <= MAX_ACTIVE_PRINTERS; attempt++) {
    uint8_t next = (current + attempt) % MAX_ACTIVE_PRINTERS;
    for (uint8_t c = 0; c < candidateCount; c++) {
      if (candidates[c] == next && next != current) {
        rotState.displayIndex = next;
        triggerDisplayTransition();
        rotState.lastRotateMs = now;
        return;
      }
    }
  }
  rotState.lastRotateMs = now;
}

// ---------------------------------------------------------------------------
//  Launcher: handle selection and dismiss
// ---------------------------------------------------------------------------
static void handleLauncherSelection(int8_t sel) {
  switch (sel) {
    case LAUNCHER_DASHBOARD:
      setScreenState(prelaunchScreen == SCREEN_LAUNCHER ? SCREEN_IDLE : prelaunchScreen);
      break;

    case LAUNCHER_CLOCK:
      setScreenState(SCREEN_CLOCK);
      break;

    case LAUNCHER_STORE:
      setScreenState(SCREEN_STORE);
      break;

    case LAUNCHER_SETTINGS:
      // Show idle screen — configure via web UI over WiFi
      setScreenState(SCREEN_IDLE);
      break;

    default:
      if (sel >= LAUNCHER_USER_BASE && sel < LAUNCHER_ITEM_COUNT) {
        uint8_t appIdx = (uint8_t)(sel - LAUNCHER_USER_BASE);
        const AppInfo* app = appManagerGetApp(appIdx);
        if (app) {
          size_t scriptLen = 0;
          char* script = appManagerLoadScript(app->id, &scriptLen);
          if (script) {
            appScreenPrepare(app->name);
            setScreenState(SCREEN_APP);
            luaRuntimeRun(script, scriptLen, app->name);
            heap_caps_free(script);
          } else {
            setScreenState(SCREEN_APP);
            appScreenPrepare(app->name);
          }
        } else {
          // Empty slot — nothing to do, restore previous screen
          setScreenState(prelaunchScreen == SCREEN_LAUNCHER ? SCREEN_IDLE : prelaunchScreen);
        }
      } else {
        // Dismiss (-2 timeout or unknown)
        setScreenState(prelaunchScreen == SCREEN_LAUNCHER ? SCREEN_IDLE : prelaunchScreen);
      }
      break;
  }
}

// ---------------------------------------------------------------------------
//  setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.printf("\n=== BambuHelper %s Starting ===\n", FW_VERSION);

  loadSettings();
  appManagerInit();
  luaRuntimeInit();
  initDisplay();   // calls lvglPortInit() internally; shows splash via LVGL
  splashEnd = millis() + 2000;
  setBacklight(brightness);
}

// ---------------------------------------------------------------------------
//  loop
// ---------------------------------------------------------------------------
void loop() {
  // ── Splash hold (2 s) — then start all subsystems ────────────────────────
  if (splashEnd > 0 && millis() > splashEnd) {
    splashEnd = 0;
    initWiFi();
    initWebServer();
    bambuClient.begin();
    initButton();
    initBuzzer();
  }
  if (splashEnd > 0) { delay(10); return; }

  // ── Always-running handlers ───────────────────────────────────────────────
  handleWiFi();
  handleWebServer();

  // ── Always pump LVGL except when pong clock owns the display ────────────
  bool pongActive = (getScreenState() == SCREEN_CLOCK && dispSettings.pongClock);
  if (!pongActive) {
    lvglPortTick();
  }

  // ── Launcher active — handle entirely here, skip rest of state machine ───
  if (getScreenState() == SCREEN_LAUNCHER) {
    int8_t sel = launcherUpdate();
    if (sel != -1) {          // selection made or auto-dismissed
      handleLauncherSelection(sel);
      applyDisplaySettings();  // re-apply rotation; LVGL invalidates current screen
    }
    // Still pump MQTT and buzzer while launcher is open
    if (isWiFiConnected() && !isAPMode() && bambuClient.isAnyConfigured()) {
      bambuClient.loop();
    }
    buzzerTick();
    return;                   // do NOT call updateDisplay() for launcher
  }

  // ── Printer list active — handle entirely here ───────────────────────────
  if (getScreenState() == SCREEN_PRINTER_LIST) {
    int8_t sel = printerListUpdate();
    if (sel >= 0) {
      // User tapped a printer — switch to it
      rotState.displayIndex = (uint8_t)sel;
      triggerDisplayTransition();
      rotState.lastRotateMs = millis();
      setScreenState(SCREEN_IDLE);
    } else if (sel == -2) {
      // Timed out — return to whichever screen was showing before the launcher
      setScreenState(prelaunchScreen == SCREEN_LAUNCHER || prelaunchScreen == SCREEN_PRINTER_LIST
                      ? SCREEN_IDLE : prelaunchScreen);
    }
    if (isWiFiConnected() && !isAPMode() && bambuClient.isAnyConfigured()) {
      bambuClient.loop();
    }
    buzzerTick();
    return;
  }

  // ── Button: long press → launcher (works regardless of WiFi state) ──────
  if (wasButtonLongPressed()) {
    prelaunchScreen = getScreenState();
    setScreenState(SCREEN_LAUNCHER);
    launcherEnter();
    buzzerTick();
    return;
  }

  // ── Button: short press ─────────────────────────────────────────────────
  if (wasButtonPressed()) {
    ScreenState cur = getScreenState();
    if (cur == SCREEN_OFF || cur == SCREEN_CLOCK) {
      setBacklight(brightness);
      finishScreenStart = 0;
      idleClockStart    = 0;
      if (isWiFiConnected() && !isAPMode()) bambuClient.resetBackoff();
      setScreenState(SCREEN_IDLE);
    } else if (isWiFiConnected() && !isAPMode() && bambuClient.activeCount() >= 2) {
      uint8_t idx = rotState.displayIndex;
      for (uint8_t a = 1; a <= MAX_ACTIVE_PRINTERS; a++) {
        uint8_t next = (idx + a) % MAX_ACTIVE_PRINTERS;
        if (bambuClient.isConfigured(next) && next != idx) {
          rotState.displayIndex = next;
          triggerDisplayTransition();
          rotState.lastRotateMs = millis();
          finishScreenStart     = 0;
          break;
        }
      }
    }
  }

  // ── WiFi / MQTT ───────────────────────────────────────────────────────────
  if (isWiFiConnected() && !isAPMode()) {
    if (bambuClient.isAnyConfigured()) {
      bambuClient.loop();
      handleRotation();
    }

    // ── Auto screen selection ───────────────────────────────────────────
    const BambuState& s = displayedPrinter().state;
    ScreenState current = getScreenState();

    if (!bambuClient.isAnyConfigured()) {
      // No printer configured — stay on IDLE (web UI shows setup prompt)
      if (current != SCREEN_IDLE) { setScreenState(SCREEN_IDLE); finishScreenStart = 0; }

    } else if (!s.connected && current != SCREEN_CONNECTING_MQTT &&
               current != SCREEN_OFF && current != SCREEN_CLOCK) {
      setScreenState(SCREEN_CONNECTING_MQTT);
      finishScreenStart = 0;

    } else if (!s.connected && (current == SCREEN_OFF || current == SCREEN_CLOCK)) {
      // Stay off/clock — printer is unreachable

    } else if (s.connected && s.printing) {
      if (current != SCREEN_PRINTING) {
        setScreenState(SCREEN_PRINTING);
        finishScreenStart = 0;
      }
      displayedPrinter().state.finishBuzzerPlayed = false;

    } else if (s.connected && !s.printing &&
               strcmp(s.gcodeState, "FINISH") == 0) {
      if (current != SCREEN_FINISHED && current != SCREEN_OFF && current != SCREEN_CLOCK) {
        setScreenState(SCREEN_FINISHED);
        finishScreenStart = millis();
        BambuState& ms = displayedPrinter().state;
        if (!ms.finishBuzzerPlayed) {
          buzzerPlay(BUZZ_PRINT_FINISHED);
          ms.finishBuzzerPlayed = true;
        }
      }
      // Transition off/clock after finish-display timeout
      if (current == SCREEN_FINISHED && !dpSettings.keepDisplayOn &&
          dpSettings.finishDisplayMins > 0 && finishScreenStart > 0 &&
          millis() - finishScreenStart > (unsigned long)dpSettings.finishDisplayMins * 60000UL) {
        bool anyPrinting = false;
        for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
          if (bambuClient.isConfigured(i) && bambuClient.getState(i).printing) {
            anyPrinting = true; break;
          }
        }
        if (!anyPrinting) {
          if (dpSettings.showClockAfterFinish) {
            setScreenState(SCREEN_CLOCK);
          } else {
            setScreenState(SCREEN_OFF);
          }
        }
      }

    } else if (s.connected && !s.printing &&
               strcmp(s.gcodeState, "FINISH") != 0) {
      if (current == SCREEN_CLOCK || current == SCREEN_OFF) {
        // Stay — idle/clock persists until button press
      } else if (current != SCREEN_IDLE) {
        setScreenState(SCREEN_IDLE);
        finishScreenStart = 0;
        idleClockStart    = 0;
      }
    }
  }

  // ── Idle → Clock auto-transition ─────────────────────────────────────────
  ScreenState cur = getScreenState();
  if (cur == SCREEN_IDLE && dpSettings.showClockAfterFinish &&
      !dpSettings.keepDisplayOn && dpSettings.finishDisplayMins > 0) {
    bool anyBusy = false;
    for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
      if (bambuClient.isConfigured(i) && bambuClient.getState(i).printing) {
        anyBusy = true; break;
      }
    }
    if (!anyBusy) {
      if (idleClockStart == 0) idleClockStart = millis();
      if (millis() - idleClockStart > (unsigned long)dpSettings.finishDisplayMins * 60000UL) {
        setScreenState(SCREEN_CLOCK);
      }
    } else {
      idleClockStart = 0;
    }
  } else if (cur != SCREEN_IDLE) {
    idleClockStart = 0;
  }

  // ── Error buzzer ──────────────────────────────────────────────────────────
  for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
    if (!bambuClient.isConfigured(i)) continue;
    const BambuState& ps = bambuClient.getState(i);
    if (strcmp(ps.gcodeState, "FAILED") == 0 &&
        strcmp(prevGcodeState[i], "FAILED") != 0 &&
        prevGcodeState[i][0] != '\0') {
      buzzerPlay(BUZZ_ERROR);
    }
    strlcpy(prevGcodeState[i], ps.gcodeState, sizeof(prevGcodeState[i]));
  }

  buzzerTick();
  updateDisplay();
}

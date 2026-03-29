/*
 * screen_boot.cpp — LVGL screens for boot and connection states
 *
 * Covers: SCREEN_SPLASH, SCREEN_AP_MODE, SCREEN_CONNECTING_WIFI,
 *         SCREEN_WIFI_CONNECTED, SCREEN_CONNECTING_MQTT, SCREEN_OFF
 *
 * All screens are allocated once at boot and reused (never freed).
 */

#include "screen_boot.h"
#include "config.h"
#include "settings.h"
#include "bambu_mqtt.h"
#include <WiFi.h>
#include <lvgl.h>

// ---------------------------------------------------------------------------
//  Persistent screen objects
// ---------------------------------------------------------------------------
static lv_obj_t* g_scr_splash  = nullptr;
static lv_obj_t* g_scr_ap      = nullptr;
static lv_obj_t* g_scr_cwifi   = nullptr;  // connecting WiFi
static lv_obj_t* g_scr_wconn   = nullptr;  // WiFi connected
static lv_obj_t* g_scr_cmqtt   = nullptr;  // connecting MQTT
static lv_obj_t* g_scr_off     = nullptr;

// Dynamic labels on the WiFi-connected screen
static lv_obj_t* g_lbl_ip      = nullptr;

// Dynamic labels on the connecting-MQTT screen
static lv_obj_t* g_lbl_cmqtt_info    = nullptr;
static lv_obj_t* g_lbl_cmqtt_elapsed = nullptr;
static lv_obj_t* g_lbl_cmqtt_attempt = nullptr;
static lv_obj_t* g_lbl_cmqtt_error   = nullptr;

// Timestamp when SCREEN_CONNECTING_MQTT was entered
static unsigned long g_cmqtt_start_ms = 0;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

// Convert RGB565 (used throughout the codebase) to lv_color_t
static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

// Create a screen whose background matches dispSettings.bgColor
static lv_obj_t* makeScreen() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, c565(dispSettings.bgColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    return scr;
}

// Create a centered label with specified font and color
static lv_obj_t* centeredLabel(lv_obj_t* parent, const char* text,
                                const lv_font_t* font, lv_color_t color,
                                lv_coord_t y_ofs) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl, 220);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, y_ofs);
    return lbl;
}

// Create an lv_spinner sized and centered with y_ofs
static lv_obj_t* makeSpinner(lv_obj_t* parent, lv_color_t color, lv_coord_t y_ofs) {
    lv_obj_t* sp = lv_spinner_create(parent, 1000, 60);
    lv_obj_set_size(sp, 52, 52);
    lv_obj_align(sp, LV_ALIGN_CENTER, 0, y_ofs);
    lv_obj_set_style_arc_color(sp, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sp, c565(dispSettings.trackColor), LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_MAIN);
    return sp;
}

// ---------------------------------------------------------------------------
//  SCREEN_SPLASH
// ---------------------------------------------------------------------------
static void buildSplash() {
    g_scr_splash = makeScreen();

    centeredLabel(g_scr_splash, "BambuHelper",
                  &lv_font_montserrat_28, c565(CLR_GREEN), -20);
    centeredLabel(g_scr_splash, "Printer Monitor",
                  &lv_font_montserrat_16, c565(CLR_TEXT_DIM), 14);
    centeredLabel(g_scr_splash, FW_VERSION,
                  &lv_font_montserrat_14, c565(CLR_TEXT_DIM), 36);
}

// ---------------------------------------------------------------------------
//  SCREEN_AP_MODE
// ---------------------------------------------------------------------------
static void buildAPMode() {
    g_scr_ap = makeScreen();

    centeredLabel(g_scr_ap, "WiFi Setup",
                  &lv_font_montserrat_28, c565(CLR_GREEN), -80);
    centeredLabel(g_scr_ap, "Connect to WiFi:",
                  &lv_font_montserrat_16, c565(CLR_TEXT), -46);

    // SSID from MAC — computed once at boot
    char ssid[32];
    uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
    snprintf(ssid, sizeof(ssid), "%s%04X", WIFI_AP_PREFIX, mac);
    centeredLabel(g_scr_ap, ssid,
                  &lv_font_montserrat_20, c565(CLR_CYAN), -18);

    centeredLabel(g_scr_ap, "Password:",
                  &lv_font_montserrat_14, c565(CLR_TEXT_DIM), 10);
    centeredLabel(g_scr_ap, WIFI_AP_PASSWORD,
                  &lv_font_montserrat_16, c565(CLR_TEXT), 30);

    centeredLabel(g_scr_ap, "Then open:",
                  &lv_font_montserrat_14, c565(CLR_TEXT_DIM), 58);
    centeredLabel(g_scr_ap, "192.168.4.1",
                  &lv_font_montserrat_20, c565(CLR_ORANGE), 82);
}

// ---------------------------------------------------------------------------
//  SCREEN_CONNECTING_WIFI
// ---------------------------------------------------------------------------
static void buildConnectingWiFi() {
    g_scr_cwifi = makeScreen();

    centeredLabel(g_scr_cwifi, "Connecting to WiFi",
                  &lv_font_montserrat_16, c565(CLR_TEXT), -46);
    makeSpinner(g_scr_cwifi, c565(CLR_BLUE), 10);
}

// ---------------------------------------------------------------------------
//  SCREEN_WIFI_CONNECTED
// ---------------------------------------------------------------------------
static void buildWiFiConnected() {
    g_scr_wconn = makeScreen();

    // Green circle with checkmark
    lv_obj_t* circle = lv_obj_create(g_scr_wconn);
    lv_obj_set_size(circle, 52, 52);
    lv_obj_set_style_bg_color(circle, c565(CLR_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(circle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(circle, 0, LV_PART_MAIN);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, -46);

    lv_obj_t* ok = lv_label_create(circle);
    lv_label_set_text(ok, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(ok, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ok, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ok, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_center(ok);

    centeredLabel(g_scr_wconn, "WiFi Connected",
                  &lv_font_montserrat_20, c565(CLR_GREEN), 10);

    // IP address — updated in bootScreenPrepare / bootScreenUpdate
    g_lbl_ip = centeredLabel(g_scr_wconn, "",
                              &lv_font_montserrat_16, c565(CLR_TEXT), 38);
}

// ---------------------------------------------------------------------------
//  SCREEN_CONNECTING_MQTT
// ---------------------------------------------------------------------------
static void buildConnectingMQTT() {
    g_scr_cmqtt = makeScreen();

    centeredLabel(g_scr_cmqtt, "Connecting to Printer",
                  &lv_font_montserrat_16, c565(CLR_TEXT), -76);
    makeSpinner(g_scr_cmqtt, c565(CLR_ORANGE), -26);

    g_lbl_cmqtt_info = centeredLabel(g_scr_cmqtt, "",
                                      &lv_font_montserrat_14, c565(CLR_TEXT_DIM), 14);
    g_lbl_cmqtt_elapsed = centeredLabel(g_scr_cmqtt, "",
                                         &lv_font_montserrat_14, c565(CLR_TEXT_DIM), 36);
    g_lbl_cmqtt_attempt = centeredLabel(g_scr_cmqtt, "",
                                         &lv_font_montserrat_14, c565(CLR_TEXT_DIM), 54);
    g_lbl_cmqtt_error   = centeredLabel(g_scr_cmqtt, "",
                                         &lv_font_montserrat_14, c565(CLR_RED), 70);
}

// ---------------------------------------------------------------------------
//  SCREEN_OFF
// ---------------------------------------------------------------------------
static void buildOff() {
    g_scr_off = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr_off, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_scr_off, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_scr_off, 0, LV_PART_MAIN);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void bootScreensInit() {
    buildSplash();
    buildAPMode();
    buildConnectingWiFi();
    buildWiFiConnected();
    buildConnectingMQTT();
    buildOff();
}

lv_obj_t* bootScreenGet(ScreenState state) {
    switch (state) {
        case SCREEN_SPLASH:          return g_scr_splash;
        case SCREEN_AP_MODE:         return g_scr_ap;
        case SCREEN_CONNECTING_WIFI: return g_scr_cwifi;
        case SCREEN_WIFI_CONNECTED:  return g_scr_wconn;
        case SCREEN_CONNECTING_MQTT: return g_scr_cmqtt;
        case SCREEN_OFF:             return g_scr_off;
        default:                     return nullptr;
    }
}

void bootScreenPrepare(ScreenState state) {
    if (state == SCREEN_WIFI_CONNECTED) {
        lv_label_set_text(g_lbl_ip, WiFi.localIP().toString().c_str());
        lv_obj_align(g_lbl_ip, LV_ALIGN_CENTER, 0, 38);
    }
    if (state == SCREEN_CONNECTING_MQTT) {
        g_cmqtt_start_ms = millis();
        lv_label_set_text(g_lbl_cmqtt_elapsed, "");
        lv_label_set_text(g_lbl_cmqtt_attempt, "");
        lv_label_set_text(g_lbl_cmqtt_error,   "");
        // Info line (printer mode + IP/serial) — update now while we have context
        bootScreenUpdate(SCREEN_CONNECTING_MQTT);
    }
}

void bootScreenUpdate(ScreenState state) {
    if (state == SCREEN_WIFI_CONNECTED) {
        // Refresh IP in case DHCP renewed
        lv_label_set_text(g_lbl_ip, WiFi.localIP().toString().c_str());
        return;
    }

    if (state != SCREEN_CONNECTING_MQTT) return;

    // Printer info line
    PrinterSlot& p = displayedPrinter();
    const char* modeStr = isCloudMode(p.config.mode) ? "Cloud" : "LAN";
    char infoBuf[48];
    if (isCloudMode(p.config.mode)) {
        snprintf(infoBuf, sizeof(infoBuf), "[%s] %s", modeStr,
                 p.config.serial[0] ? p.config.serial : "no serial");
    } else {
        snprintf(infoBuf, sizeof(infoBuf), "[%s] %s", modeStr,
                 p.config.ip[0] ? p.config.ip : "no IP");
    }
    lv_label_set_text(g_lbl_cmqtt_info, infoBuf);

    // Elapsed time
    if (g_cmqtt_start_ms > 0) {
        unsigned long elapsed = (millis() - g_cmqtt_start_ms) / 1000;
        char elBuf[16];
        snprintf(elBuf, sizeof(elBuf), "%lus", elapsed);
        lv_label_set_text(g_lbl_cmqtt_elapsed, elBuf);
    }

    // Attempt count + error from MQTT diagnostics
    const MqttDiag& d = getMqttDiag(rotState.displayIndex);
    if (d.attempts > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Attempt: %u", d.attempts);
        lv_label_set_text(g_lbl_cmqtt_attempt, buf);

        if (d.lastRc != 0) {
            char errBuf[32];
            snprintf(errBuf, sizeof(errBuf), "Err: %s", mqttRcToString(d.lastRc));
            lv_label_set_text(g_lbl_cmqtt_error, errBuf);
        } else {
            lv_label_set_text(g_lbl_cmqtt_error, "");
        }
    }
}

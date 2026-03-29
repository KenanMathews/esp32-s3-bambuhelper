/*
 * screen_info.cpp — Device info screen (Settings tile)
 *
 * Round display layout (240×240):
 *
 *       v2.4-ws240          ← version, blue, top centre
 *   ────────────────────
 *   SSID   MyNetwork        ← WiFi name
 *   Sig    Good (-65 dBm)   ← signal quality
 *   IP     192.168.1.42
 *   Heap   142 KB
 *   MQTT   1 printer
 *   ────────────────────
 *        192.168.1.42       ← web UI IP, blue, bottom
 */

#include "screen_info.h"
#include "config.h"
#include "settings.h"
#include "bambu_state.h"
#include "bambu_mqtt.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

static inline lv_obj_t* makeDivider(lv_obj_t* parent, int16_t y) {
    lv_obj_t* d = lv_obj_create(parent);
    lv_obj_set_size(d, 196, 1);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(d, c565(CLR_TEXT_DARK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(d, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(d, 0, LV_PART_MAIN);
    return d;
}

// ---------------------------------------------------------------------------
//  Row: dim key on left, bright value on right.  Returns the value label.
// ---------------------------------------------------------------------------
#define KEY_X   22
#define VAL_X   74
#define VAL_W   144   // 74 + 144 = 218, leaves margin for round corners

static lv_obj_t* makeRow(lv_obj_t* parent, const char* key, int16_t y) {
    lv_obj_t* k = lv_label_create(parent);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_font(k, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(k, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_pos(k, KEY_X, y);

    lv_obj_t* v = lv_label_create(parent);
    lv_label_set_text(v, "");
    lv_obj_set_style_text_font(v, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(v, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_set_pos(v, VAL_X, y);
    lv_obj_set_width(v, VAL_W);
    lv_label_set_long_mode(v, LV_LABEL_LONG_CLIP);
    return v;
}

// ---------------------------------------------------------------------------
static lv_obj_t* g_screen   = nullptr;
static lv_obj_t* g_lbl_ssid = nullptr;
static lv_obj_t* g_lbl_sig  = nullptr;
static lv_obj_t* g_lbl_heap = nullptr;
static lv_obj_t* g_lbl_mqtt = nullptr;
static lv_obj_t* g_lbl_webui = nullptr;

// ---------------------------------------------------------------------------
void infoScreenInit() {
    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_screen, 0, LV_PART_MAIN);

    // Version — top centre, blue
    lv_obj_t* ver = lv_label_create(g_screen);
    lv_label_set_text(ver, FW_VERSION);
    lv_obj_set_style_text_font(ver, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, c565(CLR_BLUE), LV_PART_MAIN);
    lv_obj_align(ver, LV_ALIGN_TOP_MID, 0, 14);

    makeDivider(g_screen, 44);

    // Rows — 22px pitch
    g_lbl_ssid = makeRow(g_screen, "SSID", 52);
    g_lbl_sig  = makeRow(g_screen, "Sig",  74);
    g_lbl_heap = makeRow(g_screen, "Heap", 96);
    g_lbl_mqtt = makeRow(g_screen, "MQTT", 118);

    makeDivider(g_screen, 163);

    // Web UI label
    lv_obj_t* hint = lv_label_create(g_screen);
    lv_label_set_text(hint, "Web UI");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 170);

    g_lbl_webui = lv_label_create(g_screen);
    lv_label_set_text(g_lbl_webui, "---");
    lv_obj_set_style_text_font(g_lbl_webui, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_webui, c565(CLR_BLUE), LV_PART_MAIN);
    lv_obj_align(g_lbl_webui, LV_ALIGN_TOP_MID, 0, 186);
    lv_obj_set_width(g_lbl_webui, 200);
    lv_obj_set_style_text_align(g_lbl_webui, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // AP mode button — bottom centre
    lv_obj_t* btn = lv_btn_create(g_screen);
    lv_obj_set_size(btn, 140, 28);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_color(btn, c565(CLR_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, c565(CLR_BTN_PR), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        if (isAPMode()) {
            reconnectWiFi();
        } else {
            disconnectBambuMqtt();
            forceAPMode();
        }
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Setup / AP Toggle");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_lbl, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_center(btn_lbl);
}

lv_obj_t* infoScreenGet() { return g_screen; }

void infoScreenEnter() {
    infoScreenUpdate();
    lv_scr_load(g_screen);
}

void infoScreenUpdate() {
    if (!g_screen) return;

    if (isWiFiConnected()) {
        // SSID
        lv_label_set_text(g_lbl_ssid, wifiSSID);

        // Signal quality — plain ASCII, no special chars
        int32_t rssi = WiFi.RSSI();
        char sigBuf[24];
        const char* quality = rssi >= -60 ? "Excellent" :
                              rssi >= -70 ? "Good"      :
                              rssi >= -80 ? "Fair"      : "Weak";
        snprintf(sigBuf, sizeof(sigBuf), "%s (%ld dBm)", quality, (long)rssi);
        lv_label_set_text(g_lbl_sig, sigBuf);
        lv_obj_set_style_text_color(g_lbl_sig,
            c565(rssi >= -70 ? CLR_GREEN : rssi >= -80 ? CLR_YELLOW : CLR_RED),
            LV_PART_MAIN);

        // Web UI
        lv_label_set_text(g_lbl_webui, WiFi.localIP().toString().c_str());
        lv_obj_set_style_text_color(g_lbl_webui, c565(CLR_BLUE), LV_PART_MAIN);

    } else if (isAPMode()) {
        lv_label_set_text(g_lbl_ssid, getAPSSID().c_str());
        lv_label_set_text(g_lbl_sig,  "Hotspot active");
        lv_obj_set_style_text_color(g_lbl_sig, c565(CLR_YELLOW), LV_PART_MAIN);
        lv_label_set_text(g_lbl_webui, "192.168.4.1");
        lv_obj_set_style_text_color(g_lbl_webui, c565(CLR_YELLOW), LV_PART_MAIN);
    } else {
        // Show configured SSID so user knows what it's trying to connect to
        lv_label_set_text(g_lbl_ssid, wifiSSID[0] ? wifiSSID : "Not set");
        lv_label_set_text(g_lbl_sig,  "Disconnected");
        lv_obj_set_style_text_color(g_lbl_sig, c565(CLR_TEXT_DIM), LV_PART_MAIN);
        lv_label_set_text(g_lbl_webui, "---");
        lv_obj_set_style_text_color(g_lbl_webui, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    }

    // Heap
    char heapBuf[16];
    snprintf(heapBuf, sizeof(heapBuf), "%u KB", ESP.getFreeHeap() / 1024);
    lv_label_set_text(g_lbl_heap, heapBuf);

    // MQTT connected printer count
    uint8_t conn = 0;
    for (uint8_t i = 0; i < MAX_PRINTERS; i++) {
        if (printers[i].state.connected) conn++;
    }
    char mqttBuf[20];
    if (conn == 0)
        snprintf(mqttBuf, sizeof(mqttBuf), "No printers");
    else
        snprintf(mqttBuf, sizeof(mqttBuf), "%d printer%s", conn, conn > 1 ? "s" : "");
    lv_label_set_text(g_lbl_mqtt, mqttBuf);
}

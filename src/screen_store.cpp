#include "screen_store.h"
#include "app_manager.h"
#include "lvgl_port.h"
#include "wifi_manager.h"
#include "config.h"
#include <Arduino.h>
#include <lvgl.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// ---------------------------------------------------------------------------
//  Color helper
// ---------------------------------------------------------------------------
static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------
#define STORE_MAX_APPS    12
#define ROW_H             46    // taller row to fit name + description
#define LIST_Y_START      56    // below title (y=10,h=24) + status (y=36,h=18) + gap
#define STORE_SOURCE_URL  "https://raw.githubusercontent.com/KenanMathews/esp32-s3-bambuhelper/main/store/index.json"

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
enum StorePhase : uint8_t {
    PHASE_IDLE,
    PHASE_FETCHING,
    PHASE_SHOW_LIST,
    PHASE_INSTALLING,
    PHASE_ERROR
};

struct StoreEntry {
    char     id[APP_ID_LEN];
    char     name[APP_NAME_LEN];
    char     description[64];
    char     author[32];
    char     category[16];
    char     script_url[128];
    char     version[12];
    uint8_t  sdk_min;
    uint16_t color;
    bool     installed;
};

static lv_obj_t*     g_screen      = nullptr;
static lv_obj_t*     g_status_lbl  = nullptr;
static lv_obj_t*     g_list        = nullptr;
static StorePhase    g_phase       = PHASE_IDLE;
static StoreEntry    g_entries[STORE_MAX_APPS];
static uint8_t       g_entry_count = 0;
static unsigned long g_fetch_start = 0;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static void setStatus(const char* msg, uint16_t color = CLR_TEXT_DIM) {
    if (!g_status_lbl) return;
    lv_label_set_text(g_status_lbl, msg);
    lv_obj_set_style_text_color(g_status_lbl, c565(color), LV_PART_MAIN);
}

static bool isInstalled(const char* id) {
    uint8_t count = appManagerCount();
    for (uint8_t i = 0; i < count; i++) {
        const AppInfo* app = appManagerGetApp(i);
        if (app && strcmp(app->id, id) == 0) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Build blank screen shell
// ---------------------------------------------------------------------------
static void buildShell() {
    if (g_screen) {
        lv_obj_clean(g_screen);
    } else {
        g_screen = lv_obj_create(NULL);
    }
    lv_obj_set_style_bg_color(g_screen, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_screen, 0, LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(g_screen);
    lv_label_set_text(title, "App Store");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    g_status_lbl = lv_label_create(g_screen);
    lv_label_set_text(g_status_lbl, "");
    lv_obj_set_style_text_font(g_status_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_status_lbl, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_align(g_status_lbl, LV_ALIGN_TOP_MID, 0, 36);

    g_list = nullptr;
}

// ---------------------------------------------------------------------------
//  Install callback
// ---------------------------------------------------------------------------
static void install_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= g_entry_count) return;

    g_phase = PHASE_INSTALLING;
    setStatus("Installing...", CLR_YELLOW);
    lvglPortTick();

    HTTPClient http;
    http.begin(g_entries[idx].script_url);
    http.setTimeout(10000);
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        char path[64];
        snprintf(path, sizeof(path), "/apps/%s.lua", g_entries[idx].id);
        LittleFS.mkdir("/apps");
        File f = LittleFS.open(path, "w");
        if (f) {
            f.print(body);
            f.close();
            appManagerInit();
            g_entries[idx].installed = true;
            setStatus("Installed!", CLR_GREEN);
        } else {
            setStatus("Write error", CLR_RED);
        }
    } else {
        char msg[24];
        snprintf(msg, sizeof(msg), "HTTP %d", code);
        setStatus(msg, CLR_RED);
    }
    http.end();
    g_phase = PHASE_SHOW_LIST;
}

// ---------------------------------------------------------------------------
//  Delete callback
// ---------------------------------------------------------------------------
static void delete_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= g_entry_count) return;
    if (appManagerDelete(g_entries[idx].id)) {
        g_entries[idx].installed = false;
        setStatus("Deleted", CLR_TEXT_DIM);
    }
}

// ---------------------------------------------------------------------------
//  Build scrollable app list
// ---------------------------------------------------------------------------
static void buildList() {
    if (g_list) { lv_obj_del(g_list); g_list = nullptr; }

    g_list = lv_obj_create(g_screen);
    lv_obj_set_size(g_list, 232, 240 - LIST_Y_START - 4);
    lv_obj_align(g_list, LV_ALIGN_TOP_MID, 0, LIST_Y_START);
    lv_obj_set_style_bg_color(g_list, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(g_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < g_entry_count; i++) {
        const StoreEntry& entry = g_entries[i];

        lv_obj_t* row = lv_obj_create(g_list);
        lv_obj_set_size(row, 220, ROW_H);
        lv_obj_set_style_bg_color(row, c565(i % 2 == 0 ? CLR_CARD : CLR_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);

        // Color dot
        lv_obj_t* dot = lv_obj_create(row);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_pos(dot, 6, (ROW_H - 8) / 2);
        lv_obj_set_style_bg_color(dot, c565(entry.color ? entry.color : CLR_BTN), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(dot, 4, LV_PART_MAIN);

        // App name (top line)
        lv_obj_t* name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, entry.name);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(name_lbl, c565(CLR_TEXT), LV_PART_MAIN);
        lv_obj_set_pos(name_lbl, 20, 4);

        // Description (bottom line)
        lv_obj_t* desc_lbl = lv_label_create(row);
        lv_label_set_text(desc_lbl, entry.description[0] ? entry.description : entry.category);
        lv_obj_set_style_text_font(desc_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(desc_lbl, c565(CLR_TEXT_DIM), LV_PART_MAIN);
        lv_obj_set_pos(desc_lbl, 20, 22);
        lv_obj_set_width(desc_lbl, 120);
        lv_label_set_long_mode(desc_lbl, LV_LABEL_LONG_CLIP);

        // Version (top-right)
        lv_obj_t* ver_lbl = lv_label_create(row);
        lv_label_set_text(ver_lbl, entry.version);
        lv_obj_set_style_text_font(ver_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(ver_lbl, c565(CLR_TEXT_DARK), LV_PART_MAIN);
        lv_obj_align(ver_lbl, LV_ALIGN_RIGHT_MID, -50, -8);

        // Action button
        lv_obj_t* btn = lv_btn_create(row);
        lv_obj_set_size(btn, 44, 26);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -2, 0);
        lv_obj_set_style_radius(btn, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);

        if (entry.installed) {
            lv_obj_set_style_bg_color(btn, c565(CLR_RED), LV_PART_MAIN);
            lv_obj_add_event_cb(btn, delete_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        } else {
            lv_obj_set_style_bg_color(btn, c565(CLR_GREEN), LV_PART_MAIN);
            lv_obj_add_event_cb(btn, install_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        }

        lv_obj_t* btn_lbl = lv_label_create(btn);
        lv_label_set_text(btn_lbl, entry.installed ? "Del" : "Get");
        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(btn_lbl, c565(CLR_TEXT), LV_PART_MAIN);
        lv_obj_center(btn_lbl);
    }

    if (g_entry_count == 0) {
        lv_obj_t* empty = lv_label_create(g_list);
        lv_label_set_text(empty, "No apps found");
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, c565(CLR_TEXT_DIM), LV_PART_MAIN);
        lv_obj_center(empty);
    }
}

// ---------------------------------------------------------------------------
//  Parse index.json
// ---------------------------------------------------------------------------
static void parseIndex(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        setStatus("JSON error", CLR_RED);
        g_phase = PHASE_ERROR;
        return;
    }

    JsonArray apps = doc["apps"].as<JsonArray>();
    g_entry_count = 0;
    for (JsonObject app : apps) {
        if (g_entry_count >= STORE_MAX_APPS) break;
        StoreEntry& ent = g_entries[g_entry_count];
        strlcpy(ent.id,          app["id"]          | "", sizeof(ent.id));
        strlcpy(ent.name,        app["name"]        | "", sizeof(ent.name));
        strlcpy(ent.description, app["description"] | "", sizeof(ent.description));
        strlcpy(ent.author,      app["author"]      | "", sizeof(ent.author));
        strlcpy(ent.category,    app["category"]    | "", sizeof(ent.category));
        strlcpy(ent.script_url,  app["script_url"]  | "", sizeof(ent.script_url));
        strlcpy(ent.version,     app["version"]     | "?", sizeof(ent.version));
        ent.sdk_min   = app["sdk_min"]  | 1;
        ent.color     = (uint16_t)(app["color"].as<unsigned int>());
        ent.installed = isInstalled(ent.id);
        if (ent.id[0] && ent.name[0] && ent.script_url[0]) g_entry_count++;
    }
}

// ---------------------------------------------------------------------------
//  HTTP fetch (blocking, called from update after short render delay)
// ---------------------------------------------------------------------------
static void doFetch() {
    setStatus("Fetching...", CLR_YELLOW);
    lvglPortTick();

    HTTPClient http;
    http.begin(STORE_SOURCE_URL);
    http.setTimeout(8000);
    int code = http.GET();
    if (code == 200) {
        parseIndex(http.getString());
        if (g_phase != PHASE_ERROR) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%d app%s", g_entry_count,
                     g_entry_count == 1 ? "" : "s");
            setStatus(buf, CLR_TEXT_DIM);
            buildList();
            g_phase = PHASE_SHOW_LIST;
        }
    } else {
        char msg[32];
        snprintf(msg, sizeof(msg), code < 0 ? "No network" : "HTTP %d", code);
        setStatus(msg, CLR_RED);
        g_phase = PHASE_ERROR;
    }
    http.end();
}

// ===========================================================================
//  Public API
// ===========================================================================

void storeScreenInit() {
    buildShell();
}

lv_obj_t* storeScreenGet() {
    return g_screen;
}

void storeScreenEnter() {
    buildShell();
    lv_scr_load(g_screen);
    g_phase       = PHASE_FETCHING;
    g_fetch_start = millis();

    if (!isWiFiConnected() || isAPMode()) {
        setStatus("No WiFi", CLR_RED);
        g_phase = PHASE_ERROR;
        return;
    }
    setStatus("Connecting...", CLR_YELLOW);
}

void storeScreenUpdate() {
    lvglPortTick();

    if (g_phase == PHASE_FETCHING) {
        // Wait one frame so "Connecting..." renders before blocking HTTP call
        if (millis() - g_fetch_start >= 120) {
            doFetch();
        }
    }
}

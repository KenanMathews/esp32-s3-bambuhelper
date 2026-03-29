#include "screen_printer_list.h"
#include "lvgl_port.h"
#include "bambu_mqtt.h"
#include "bambu_state.h"
#include "config.h"
#include <lvgl.h>

static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

static const unsigned long AUTO_DISMISS_MS = 5000;

static lv_obj_t* g_screen    = NULL;
static lv_obj_t* g_btns[MAX_ACTIVE_PRINTERS];
static int8_t    g_selected  = -1;
static unsigned long g_enterMs = 0;

// ---------------------------------------------------------------------------
//  Button tap callback
// ---------------------------------------------------------------------------
static void btn_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t* btn = lv_event_get_target(e);
    for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
        if (g_btns[i] == btn) { g_selected = (int8_t)i; break; }
    }
}

// ---------------------------------------------------------------------------
//  Status text and color for a printer slot
// ---------------------------------------------------------------------------
static const char* statusText(uint8_t slot) {
    if (!isPrinterConfigured(slot)) return "Not configured";
    const BambuState& s = printers[slot].state;
    if (!s.connected)              return "Offline";
    if (s.printing) {
        static char buf[16];
        snprintf(buf, sizeof(buf), "Printing %d%%", s.progress);
        return buf;
    }
    if (strcmp(s.gcodeState, "FINISH") == 0) return "Finished";
    if (strcmp(s.gcodeState, "PAUSE")  == 0) return "Paused";
    if (strcmp(s.gcodeState, "FAILED") == 0) return "Error";
    return "Ready";
}

static uint16_t statusColor(uint8_t slot) {
    if (!isPrinterConfigured(slot)) return CLR_TEXT_DARK;
    const BambuState& s = printers[slot].state;
    if (!s.connected)              return CLR_TEXT_DARK;
    if (s.printing)                return CLR_GREEN;
    if (strcmp(s.gcodeState, "FINISH") == 0) return CLR_GREEN;
    if (strcmp(s.gcodeState, "PAUSE")  == 0) return CLR_YELLOW;
    if (strcmp(s.gcodeState, "FAILED") == 0) return CLR_RED;
    return CLR_TEXT_DIM;
}

// ---------------------------------------------------------------------------
//  Build the screen
// ---------------------------------------------------------------------------
static void buildScreen() {
    if (g_screen == NULL) {
        g_screen = lv_obj_create(NULL);
    } else {
        lv_obj_clean(g_screen);
    }

    lv_obj_set_style_bg_color(g_screen, c565(CLR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_screen, 0, LV_PART_MAIN);

    // Title
    lv_obj_t* title = lv_label_create(g_screen);
    lv_label_set_text(title, "PRINTERS");
    lv_obj_set_style_text_color(title, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    // Two printer buttons, vertically centered
    // Each 160×52px, 10px gap, starting at y=58
    const int BTN_W = 160;
    const int BTN_H = 52;
    const int BTN_GAP = 10;
    const int BTN_START_Y = 58;

    for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
        bool configured = isPrinterConfigured(i);
        bool isActive   = (i == rotState.displayIndex);

        uint16_t bg  = isActive ? CLR_BTN_PR : CLR_BTN;
        uint16_t bgp = CLR_BTN_PR;
        if (!configured) { bg = CLR_BTN_DIS; bgp = CLR_BTN_DIS; }

        lv_obj_t* btn = lv_btn_create(g_screen);
        g_btns[i] = btn;
        lv_obj_set_size(btn, BTN_W, BTN_H);
        lv_obj_set_pos(btn, (240 - BTN_W) / 2, BTN_START_Y + i * (BTN_H + BTN_GAP));
        lv_obj_set_style_bg_color(btn, c565(bg),  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, c565(bgp), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);

        // Printer name
        const char* name = (configured && printers[i].config.name[0])
                            ? printers[i].config.name
                            : (configured ? "Printer" : "---");
        lv_obj_t* lbl_name = lv_label_create(btn);
        lv_label_set_text(lbl_name, name);
        lv_obj_set_style_text_color(lbl_name,
            c565(configured ? CLR_TEXT : CLR_TEXT_DARK), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(lbl_name, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_align(lbl_name, LV_ALIGN_CENTER, 0, -9);

        // Status line
        lv_obj_t* lbl_status = lv_label_create(btn);
        lv_label_set_text(lbl_status, statusText(i));
        lv_obj_set_style_text_color(lbl_status, c565(statusColor(i)), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(lbl_status, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 9);

        if (configured) {
            lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
        } else {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
    }
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void printerListEnter() {
    g_enterMs  = millis();
    g_selected = -1;
    buildScreen();
    lv_scr_load(g_screen);
    lvglPortTick();
}

int8_t printerListUpdate() {
    lvglPortTick();

    if (millis() - g_enterMs > AUTO_DISMISS_MS) return -2;

    if (g_selected >= 0) {
        int8_t result = g_selected;
        g_selected = -1;
        return result;
    }
    return -1;
}

/*
 * screen_printer.cpp — LVGL-based printer dashboard
 *
 * One persistent screen serves SCREEN_IDLE, SCREEN_PRINTING, and SCREEN_FINISHED.
 * Widgets are shown/hidden/repositioned based on the current state.
 *
 * Arc angle convention (LVGL 8):
 *   0° = 3 o'clock, increasing clockwise
 *   Horseshoe gauge: bg_angles(330, 210) → 240° span, 120° gap at 12 o'clock
 */

#include "screen_printer.h"
#include "config.h"
#include "settings.h"
#include "bambu_mqtt.h"
#include <lvgl.h>
#include <time.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
//  Smooth gauge interpolation state
// ---------------------------------------------------------------------------
static float smoothNozzleTemp = 0;
static float smoothBedTemp    = 0;
static float smoothPartFan    = 0;
static float smoothAuxFan     = 0;
static float smoothChamberFan = 0;
static bool  smoothInited     = false;

static const float SMOOTH_ALPHA = 0.25f;
static const float SNAP_THRESH  = 0.5f;

static void smoothLerp(float& cur, float target) {
    float diff = target - cur;
    if (fabsf(diff) < SNAP_THRESH) cur = target;
    else cur += diff * SMOOTH_ALPHA;
}

static bool smoothSettled = true;  // false while any gauge is still interpolating

static void tickSmooth(const BambuState& s, bool snap) {
    if (snap || !smoothInited) {
        smoothNozzleTemp = s.nozzleTemp;
        smoothBedTemp    = s.bedTemp;
        smoothPartFan    = s.coolingFanPct;
        smoothAuxFan     = s.auxFanPct;
        smoothChamberFan = s.chamberFanPct;
        smoothInited = true;
        smoothSettled = true;
        return;
    }
    smoothLerp(smoothNozzleTemp, s.nozzleTemp);
    smoothLerp(smoothBedTemp,    s.bedTemp);
    smoothLerp(smoothPartFan,    (float)s.coolingFanPct);
    smoothLerp(smoothAuxFan,     (float)s.auxFanPct);
    smoothLerp(smoothChamberFan, (float)s.chamberFanPct);
    smoothSettled = (smoothNozzleTemp == s.nozzleTemp &&
                     smoothBedTemp    == s.bedTemp    &&
                     smoothPartFan    == (float)s.coolingFanPct &&
                     smoothAuxFan     == (float)s.auxFanPct     &&
                     smoothChamberFan == (float)s.chamberFanPct);
}

bool printerGaugesSettled() { return smoothSettled; }

// ---------------------------------------------------------------------------
//  Color helpers
// ---------------------------------------------------------------------------
static inline lv_color_t c565(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 8;
    uint8_t g = ((rgb565 >> 5)  & 0x3F) * 4;
    uint8_t b = (rgb565 & 0x1F) * 8;
    return lv_color_make(r, g, b);
}

// ---------------------------------------------------------------------------
//  Persistent LVGL objects
// ---------------------------------------------------------------------------
static lv_obj_t* g_scr          = nullptr;

// Header (y=7-25)
static lv_obj_t* g_lbl_name     = nullptr;
static lv_obj_t* g_lbl_state    = nullptr;
static lv_obj_t* g_dot[MAX_ACTIVE_PRINTERS];   // multi-printer dots

// LED progress bar (y=28-33)
static lv_obj_t* g_bar_progress = nullptr;

// Arc gauges — row 1 (progress, nozzle, bed)
static lv_obj_t* g_arc_progress = nullptr;
static lv_obj_t* g_lbl_prog_pct = nullptr;
static lv_obj_t* g_lbl_prog_rem = nullptr;
static lv_obj_t* g_lbl_progress = nullptr;   // "Progress" below arc

static lv_obj_t* g_arc_nozzle   = nullptr;
static lv_obj_t* g_lbl_nozzle_v = nullptr;   // value
static lv_obj_t* g_lbl_nozzle_t = nullptr;   // target
static lv_obj_t* g_lbl_nozzle   = nullptr;   // "Nozzle" below arc

static lv_obj_t* g_arc_bed      = nullptr;
static lv_obj_t* g_lbl_bed_v    = nullptr;
static lv_obj_t* g_lbl_bed_t    = nullptr;
static lv_obj_t* g_lbl_bed      = nullptr;   // "Bed" below arc

// Arc gauges — row 2 (fans)
static lv_obj_t* g_arc_pfan     = nullptr;
static lv_obj_t* g_lbl_pfan_v   = nullptr;
static lv_obj_t* g_lbl_pfan     = nullptr;   // "Part" below arc

static lv_obj_t* g_arc_afan     = nullptr;
static lv_obj_t* g_lbl_afan_v   = nullptr;
static lv_obj_t* g_lbl_afan     = nullptr;   // "Aux" below arc

static lv_obj_t* g_arc_cfan     = nullptr;
static lv_obj_t* g_lbl_cfan_v   = nullptr;
static lv_obj_t* g_lbl_cfan     = nullptr;   // "Chamber" below arc

// Info area
static lv_obj_t* g_lbl_eta      = nullptr;   // ETA / PAUSE / ERROR / "Print Complete!"
static lv_obj_t* g_lbl_file     = nullptr;   // subtask file name (finished screen)

// Bottom status bar (y=222-240)
static lv_obj_t* g_lbl_filament = nullptr;
static lv_obj_t* g_lbl_layer    = nullptr;
static lv_obj_t* g_lbl_speed    = nullptr;

// ---------------------------------------------------------------------------
//  Arc gauge factory
//  cx/cy are center coordinates within the screen (absolute).
//  size = bounding box width/height (arc fills to edge).
//
//  Two sizes used for circular-display layout (r=120px from center at 120,120):
//   ARC_SIZE_LG=60 — row-1 gauges (3 arcs at cy≈82): corners verified ≤115px from center
//   ARC_SIZE_SM=52 — row-2 fan gauges (3 arcs at cy≈158): corners verified ≤108px from center
// ---------------------------------------------------------------------------
static const int ARC_SIZE_LG   = 60;  // large gauges (progress/nozzle/bed)
static const int ARC_SIZE_SM   = 52;  // small gauges (fans)
static const int ARC_THICKNESS = 6;

static lv_obj_t* makeArc(lv_obj_t* parent,
                          int cx, int cy, int size,
                          lv_color_t fillColor,
                          lv_color_t trackColor,
                          int rangeMin, int rangeMax) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_pos(arc, cx - size / 2, cy - size / 2);

    // Remove default padding and border
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(arc, 0, LV_PART_MAIN);

    // Remove knob
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_size(arc, 0, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);

    // Track (background arc)
    lv_obj_set_style_arc_color(arc, trackColor, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ARC_THICKNESS, LV_PART_MAIN);

    // Indicator (fill arc)
    lv_obj_set_style_arc_color(arc, fillColor, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, ARC_THICKNESS, LV_PART_INDICATOR);

    // Horseshoe: 240° span, gap at 12 o'clock
    // LVGL 8: 0°=3 o'clock, clockwise.
    // Start at 330° (≈1 o'clock) → clockwise 240° → 210° (≈7 o'clock).
    // Gap (120°) runs CCW from 210°→270°(top)→330°.
    lv_arc_set_bg_angles(arc, 330, 210);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc, rangeMin, rangeMax);
    lv_arc_set_value(arc, rangeMin);

    return arc;
}

// Create label inside arc center (child of arc)
static lv_obj_t* arcInnerLabel(lv_obj_t* arc,
                                const lv_font_t* font,
                                lv_color_t color,
                                lv_coord_t y_ofs) {
    lv_obj_t* lbl = lv_label_create(arc);
    lv_label_set_text(lbl, "");
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, y_ofs);
    return lbl;
}

// Create label below arc (child of screen, aligned relative to arc)
static lv_obj_t* arcBelowLabel(lv_obj_t* screen, lv_obj_t* arc,
                                const char* text,
                                const lv_font_t* font,
                                lv_color_t color) {
    lv_obj_t* lbl = lv_label_create(screen);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align_to(lbl, arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    return lbl;
}

// ---------------------------------------------------------------------------
//  Reposition arc + below-label to a new center
//  Reads actual arc size so this works for both ARC_SIZE_LG and ARC_SIZE_SM.
// ---------------------------------------------------------------------------
static void moveArc(lv_obj_t* arc, lv_obj_t* belowLbl, int cx, int cy) {
    int sz = (int)lv_obj_get_width(arc);
    lv_obj_set_pos(arc, cx - sz / 2, cy - sz / 2);
    if (belowLbl) {
        lv_obj_align_to(belowLbl, arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    }
}

// ---------------------------------------------------------------------------
//  Show/hide helpers
// ---------------------------------------------------------------------------
static void show(lv_obj_t* obj) {
    if (obj) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
static void hide(lv_obj_t* obj) {
    if (obj) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------------------
//  Speed level helpers
// ---------------------------------------------------------------------------
static const char* speedLevelName(uint8_t level) {
    switch (level) {
        case 1: return "Silent";
        case 2: return "Std";
        case 3: return "Sport";
        case 4: return "Ludicr";
        default: return "---";
    }
}

static lv_color_t speedLevelColor(uint8_t level) {
    switch (level) {
        case 1: return c565(CLR_BLUE);
        case 2: return c565(CLR_GREEN);
        case 3: return c565(CLR_ORANGE);
        case 4: return c565(CLR_RED);
        default: return c565(CLR_TEXT_DIM);
    }
}

static const char* nozzleLabel(const BambuState& s) {
    if (!s.dualNozzle) return "Nozzle";
    return s.activeNozzle == 0 ? "Nozzle R" : "Nozzle L";
}

// ---------------------------------------------------------------------------
//  Build the screen once (called from printerScreenInit)
// ---------------------------------------------------------------------------
static void buildPrinterScreen() {
    lv_color_t bg    = c565(dispSettings.bgColor);
    lv_color_t track = c565(dispSettings.trackColor);

    // Screen
    g_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_scr, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_scr, 0, LV_PART_MAIN);

    // ── Header — centered, stacked (safe inside circular viewport at y≈8/22) ──
    g_lbl_name = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_name, "");
    lv_obj_set_style_text_font(g_lbl_name, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_name, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_name, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(g_lbl_name, LV_ALIGN_TOP_MID, 0, 6);

    g_lbl_state = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_state, "");
    lv_obj_set_style_text_font(g_lbl_state, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_state, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_state, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(g_lbl_state, LV_ALIGN_TOP_MID, 0, 22);

    // Multi-printer dots (up to MAX_ACTIVE_PRINTERS)
    for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
        lv_obj_t* dot = lv_obj_create(g_scr);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_bg_color(dot, c565(CLR_TEXT_DARK), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
        // Center the dot group at x=120, each dot 10px apart
        int dotX = 120 - (MAX_ACTIVE_PRINTERS * 10 / 2) + i * 10 - 3;
        lv_obj_set_pos(dot, dotX, 7);
        g_dot[i] = dot;
        hide(dot);  // hidden until multi-printer is active
    }

    // ── LED progress bar (y=38-43) — 120px wide keeps corners inside circle ──
    g_bar_progress = lv_bar_create(g_scr);
    lv_obj_set_size(g_bar_progress, 120, 5);
    lv_obj_set_pos(g_bar_progress, (240 - 120) / 2, 38);
    lv_bar_set_range(g_bar_progress, 0, 100);
    lv_bar_set_value(g_bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_bar_progress, track, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_bar_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_bar_progress, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_bar_progress, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_bar_progress, c565(dispSettings.progress.arc), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_bar_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_bar_progress, 2, LV_PART_INDICATOR);

    // ── Row 1 gauges: 3×ARC_SIZE_LG arcs at cy=82, col1=62/col2=120/col3=178
    //   Verified: corners ≤115px from display center (r=120). Safe in circle.
    const lv_font_t* valFont = &lv_font_montserrat_16;
    const lv_font_t* subFont = &lv_font_montserrat_14;
    const lv_font_t* lblFont = &lv_font_montserrat_14;

    g_arc_progress = makeArc(g_scr, 62, 82, ARC_SIZE_LG,
                              c565(dispSettings.progress.arc), track,
                              0, 100);
    g_lbl_prog_pct = arcInnerLabel(g_arc_progress, valFont,
                                    c565(dispSettings.progress.value), -7);
    g_lbl_prog_rem = arcInnerLabel(g_arc_progress, subFont,
                                    c565(CLR_TEXT_DIM), 8);
    g_lbl_progress = arcBelowLabel(g_scr, g_arc_progress, "Progress",
                                    lblFont, c565(dispSettings.progress.label));

    g_arc_nozzle = makeArc(g_scr, 120, 82, ARC_SIZE_LG,
                            c565(dispSettings.nozzle.arc), track,
                            0, 300);
    g_lbl_nozzle_v = arcInnerLabel(g_arc_nozzle, valFont,
                                    c565(dispSettings.nozzle.value), -7);
    g_lbl_nozzle_t = arcInnerLabel(g_arc_nozzle, subFont,
                                    c565(CLR_TEXT_DIM), 8);
    g_lbl_nozzle   = arcBelowLabel(g_scr, g_arc_nozzle, "Nozzle",
                                    lblFont, c565(dispSettings.nozzle.label));

    g_arc_bed = makeArc(g_scr, 178, 82, ARC_SIZE_LG,
                         c565(dispSettings.bed.arc), track,
                         0, 120);
    g_lbl_bed_v = arcInnerLabel(g_arc_bed, valFont,
                                 c565(dispSettings.bed.value), -7);
    g_lbl_bed_t = arcInnerLabel(g_arc_bed, subFont,
                                 c565(CLR_TEXT_DIM), 8);
    g_lbl_bed   = arcBelowLabel(g_scr, g_arc_bed, "Bed",
                                 lblFont, c565(dispSettings.bed.label));

    // ── Row 2 gauges: 3×ARC_SIZE_SM fan arcs at cy=158, col1=62/col2=120/col3=178
    //   Verified: corners ≤108px from display center. Safe in circle.
    g_arc_pfan = makeArc(g_scr, 62, 158, ARC_SIZE_SM,
                          c565(dispSettings.partFan.arc), track,
                          0, 100);
    g_lbl_pfan_v = arcInnerLabel(g_arc_pfan, valFont,
                                  c565(dispSettings.partFan.value), 0);
    g_lbl_pfan   = arcBelowLabel(g_scr, g_arc_pfan, "Part",
                                  lblFont, c565(dispSettings.partFan.label));

    g_arc_afan = makeArc(g_scr, 120, 158, ARC_SIZE_SM,
                          c565(dispSettings.auxFan.arc), track,
                          0, 100);
    g_lbl_afan_v = arcInnerLabel(g_arc_afan, valFont,
                                  c565(dispSettings.auxFan.value), 0);
    g_lbl_afan   = arcBelowLabel(g_scr, g_arc_afan, "Aux",
                                  lblFont, c565(dispSettings.auxFan.label));

    g_arc_cfan = makeArc(g_scr, 178, 158, ARC_SIZE_SM,
                          c565(dispSettings.chamberFan.arc), track,
                          0, 100);
    g_lbl_cfan_v = arcInnerLabel(g_arc_cfan, valFont,
                                  c565(dispSettings.chamberFan.value), 0);
    g_lbl_cfan   = arcBelowLabel(g_scr, g_arc_cfan, "Chamber",
                                  lblFont, c565(dispSettings.chamberFan.label));

    // ── Info line (y≈195 absolute; fans bottom at y=184, label ~188) ──────
    g_lbl_eta = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_eta, "");
    lv_obj_set_style_text_font(g_lbl_eta, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_eta, c565(CLR_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_eta, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_long_mode(g_lbl_eta, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(g_lbl_eta, 160);
    lv_obj_align(g_lbl_eta, LV_ALIGN_CENTER, 0, 75);

    g_lbl_file = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_file, "");
    lv_obj_set_style_text_font(g_lbl_file, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_file, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_file, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_long_mode(g_lbl_file, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(g_lbl_file, 160);
    lv_obj_align(g_lbl_file, LV_ALIGN_CENTER, 0, 93);

    // ── Bottom status bar — safe x range at y≈218 is ~65–175 ─────────────
    // Filament: left-aligned, starting at x=62
    g_lbl_filament = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_filament, "");
    lv_obj_set_style_text_font(g_lbl_filament, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_filament, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_filament, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_pos(g_lbl_filament, 62, 216);
    lv_obj_set_width(g_lbl_filament, 58);
    lv_obj_set_style_text_align(g_lbl_filament, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);

    // Layer count: centered
    g_lbl_layer = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_layer, "");
    lv_obj_set_style_text_font(g_lbl_layer, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_layer, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_layer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(g_lbl_layer, LV_ALIGN_BOTTOM_MID, 0, -6);

    // Speed: right-aligned, ending at x=178
    g_lbl_speed = lv_label_create(g_scr);
    lv_label_set_text(g_lbl_speed, "");
    lv_obj_set_style_text_font(g_lbl_speed, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lbl_speed, c565(CLR_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lbl_speed, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_pos(g_lbl_speed, 120, 216);
    lv_obj_set_width(g_lbl_speed, 58);
    lv_obj_set_style_text_align(g_lbl_speed, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
}

// ---------------------------------------------------------------------------
//  Configure widget positions and visibility for a given screen state
// ---------------------------------------------------------------------------
static void configureForState(ScreenState state) {
    if (state == SCREEN_PRINTING) {
        // Row 1: progress/nozzle/bed at cy=82, col1=62/col2=120/col3=178
        moveArc(g_arc_progress, g_lbl_progress, 62, 82);
        show(g_arc_progress); show(g_lbl_progress);
        show(g_lbl_prog_pct); show(g_lbl_prog_rem);
        show(g_bar_progress);

        moveArc(g_arc_nozzle, g_lbl_nozzle, 120, 82);
        moveArc(g_arc_bed, g_lbl_bed, 178, 82);
        show(g_arc_nozzle); show(g_lbl_nozzle);
        show(g_arc_bed); show(g_lbl_bed);
        show(g_lbl_nozzle_v); show(g_lbl_nozzle_t);
        show(g_lbl_bed_v); show(g_lbl_bed_t);

        // Fan gauges shown; below-labels hidden to leave room for ETA
        show(g_arc_pfan); hide(g_lbl_pfan); show(g_lbl_pfan_v);
        show(g_arc_afan); hide(g_lbl_afan); show(g_lbl_afan_v);
        show(g_arc_cfan); hide(g_lbl_cfan); show(g_lbl_cfan_v);

        // Bottom bar: three columns — restore filament to left-aligned narrow slot
        lv_obj_set_pos(g_lbl_filament, 62, 216);
        lv_obj_set_width(g_lbl_filament, 58);
        lv_obj_set_style_text_align(g_lbl_filament, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        show(g_lbl_filament); show(g_lbl_layer); show(g_lbl_speed);

        // File name hidden, ETA shown
        hide(g_lbl_file);
        show(g_lbl_eta);

    } else if (state == SCREEN_IDLE) {
        // No progress arc/bar
        hide(g_arc_progress); hide(g_lbl_progress);
        hide(g_lbl_prog_pct); hide(g_lbl_prog_rem);
        hide(g_bar_progress);

        // Nozzle and bed: two large arcs centered in lower half
        moveArc(g_arc_nozzle, g_lbl_nozzle, 80, 128);
        moveArc(g_arc_bed, g_lbl_bed, 160, 128);
        show(g_arc_nozzle); show(g_lbl_nozzle);
        show(g_arc_bed); show(g_lbl_bed);
        show(g_lbl_nozzle_v); show(g_lbl_nozzle_t);
        show(g_lbl_bed_v); show(g_lbl_bed_t);

        // No fans
        hide(g_arc_pfan); hide(g_lbl_pfan); hide(g_lbl_pfan_v);
        hide(g_arc_afan); hide(g_lbl_afan); hide(g_lbl_afan_v);
        hide(g_arc_cfan); hide(g_lbl_cfan); hide(g_lbl_cfan_v);

        // ETA hidden, file hidden
        hide(g_lbl_eta);
        hide(g_lbl_file);

        // Bottom bar: filament centered, full safe width
        lv_obj_align(g_lbl_filament, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_set_width(g_lbl_filament, 140);
        lv_obj_set_style_text_align(g_lbl_filament, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        show(g_lbl_filament);
        hide(g_lbl_layer);
        hide(g_lbl_speed);

    } else if (state == SCREEN_FINISHED) {
        // LED bar at 100%
        show(g_bar_progress);
        lv_bar_set_value(g_bar_progress, 100, LV_ANIM_OFF);

        // No progress arc
        hide(g_arc_progress); hide(g_lbl_progress);
        hide(g_lbl_prog_pct); hide(g_lbl_prog_rem);

        // Nozzle and bed: upper-center pair for FINISHED state
        moveArc(g_arc_nozzle, g_lbl_nozzle, 80, 90);
        moveArc(g_arc_bed, g_lbl_bed, 160, 90);
        show(g_arc_nozzle); show(g_lbl_nozzle);
        show(g_arc_bed); show(g_lbl_bed);
        show(g_lbl_nozzle_v); show(g_lbl_nozzle_t);
        show(g_lbl_bed_v); show(g_lbl_bed_t);

        // No fans
        hide(g_arc_pfan); hide(g_lbl_pfan); hide(g_lbl_pfan_v);
        hide(g_arc_afan); hide(g_lbl_afan); hide(g_lbl_afan_v);
        hide(g_arc_cfan); hide(g_lbl_cfan); hide(g_lbl_cfan_v);

        // "Print Complete!" + file name
        show(g_lbl_eta);
        show(g_lbl_file);

        // Bottom: filament centered, full safe width
        lv_obj_align(g_lbl_filament, LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_set_width(g_lbl_filament, 140);
        lv_obj_set_style_text_align(g_lbl_filament, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        show(g_lbl_filament);
        hide(g_lbl_layer);
        hide(g_lbl_speed);
    }
}

// ---------------------------------------------------------------------------
//  Update arc color from GaugeColors (for settings changes)
// ---------------------------------------------------------------------------
static void setArcColor(lv_obj_t* arc, const GaugeColors& gc) {
    lv_obj_set_style_arc_color(arc, c565(gc.arc), LV_PART_INDICATOR);
}

// ---------------------------------------------------------------------------
//  Update header bar
// ---------------------------------------------------------------------------
static void updateHeader(const PrinterSlot& slot, ScreenState state) {
    const BambuState& s = slot.state;
    const char* name = slot.config.name[0] ? slot.config.name : "Bambu";
    lv_label_set_text(g_lbl_name, name);

    // State badge
    const char* stateStr = s.gcodeState;
    lv_color_t  stateClr = c565(CLR_TEXT_DIM);
    if (state == SCREEN_FINISHED) {
        stateStr = "FINISH";
        stateClr = c565(CLR_GREEN);
    } else if (strcmp(s.gcodeState, "RUNNING") == 0)  { stateClr = c565(CLR_GREEN); }
    else if (strcmp(s.gcodeState, "PAUSE") == 0)      { stateClr = c565(CLR_YELLOW); }
    else if (strcmp(s.gcodeState, "FAILED") == 0)     { stateClr = c565(CLR_RED); }
    else if (strcmp(s.gcodeState, "PREPARE") == 0)    { stateClr = c565(CLR_BLUE); }
    else if (strcmp(s.gcodeState, "IDLE") == 0)       { stateStr = "Ready"; stateClr = c565(CLR_GREEN); }

    lv_label_set_text(g_lbl_state, stateStr);
    lv_obj_set_style_text_color(g_lbl_state, stateClr, LV_PART_MAIN);

    // Multi-printer dots
    uint8_t cnt = getActiveConnCount();
    for (uint8_t i = 0; i < MAX_ACTIVE_PRINTERS; i++) {
        if (cnt > 1 && isPrinterConfigured(i)) {
            show(g_dot[i]);
            bool active = (i == rotState.displayIndex);
            lv_obj_set_style_bg_color(g_dot[i],
                active ? c565(CLR_GREEN) : c565(CLR_TEXT_DARK),
                LV_PART_MAIN);
        } else {
            hide(g_dot[i]);
        }
    }
}

// ---------------------------------------------------------------------------
//  Update ETA line (printing state)
// ---------------------------------------------------------------------------
static void updateETA(const BambuState& s) {
    if (strcmp(s.gcodeState, "PAUSE") == 0) {
        lv_obj_set_style_text_color(g_lbl_eta, c565(CLR_YELLOW), LV_PART_MAIN);
        lv_label_set_text(g_lbl_eta, "PAUSED");
        return;
    }
    if (strcmp(s.gcodeState, "FAILED") == 0) {
        lv_obj_set_style_text_color(g_lbl_eta, c565(CLR_RED), LV_PART_MAIN);
        lv_label_set_text(g_lbl_eta, "ERROR!");
        return;
    }
    lv_obj_set_style_text_color(g_lbl_eta, c565(CLR_GREEN), LV_PART_MAIN);
    if (s.remainingMinutes == 0) {
        lv_label_set_text(g_lbl_eta, "ETA: ---");
        return;
    }

    static bool ntpSynced = false;
    time_t nowEpoch = time(nullptr);
    struct tm now;
    localtime_r(&nowEpoch, &now);
    if (now.tm_year > (2020 - 1900)) ntpSynced = true;

    if (ntpSynced) {
        time_t etaEpoch = nowEpoch + (time_t)s.remainingMinutes * 60;
        struct tm eta;
        localtime_r(&etaEpoch, &eta);

        int h = eta.tm_hour;
        const char* ampm = "";
        if (!netSettings.use24h) {
            ampm = h < 12 ? "AM" : "PM";
            h = h % 12;
            if (h == 0) h = 12;
        }
        char buf[32];
        if (eta.tm_yday != now.tm_yday || eta.tm_year != now.tm_year) {
            if (netSettings.use24h)
                snprintf(buf, sizeof(buf), "ETA: %d.%02d %02d:%02d",
                         eta.tm_mday, eta.tm_mon + 1, h, eta.tm_min);
            else
                snprintf(buf, sizeof(buf), "ETA: %d/%02d %d:%02d%s",
                         eta.tm_mon + 1, eta.tm_mday, h, eta.tm_min, ampm);
        } else {
            if (netSettings.use24h)
                snprintf(buf, sizeof(buf), "ETA: %02d:%02d", h, eta.tm_min);
            else
                snprintf(buf, sizeof(buf), "ETA: %d:%02d %s", h, eta.tm_min, ampm);
        }
        lv_label_set_text(g_lbl_eta, buf);
    } else {
        uint16_t h = s.remainingMinutes / 60;
        uint16_t m = s.remainingMinutes % 60;
        char buf[24];
        snprintf(buf, sizeof(buf), "Remaining: %dh %02dm", h, m);
        lv_obj_set_style_text_color(g_lbl_eta, c565(CLR_TEXT), LV_PART_MAIN);
        lv_label_set_text(g_lbl_eta, buf);
    }
}

// ---------------------------------------------------------------------------
//  Update bottom bar for PRINTING state
// ---------------------------------------------------------------------------
static void updateBottomPrinting(const BambuState& s) {
    // Layer
    char layerBuf[20];
    snprintf(layerBuf, sizeof(layerBuf), "L%d/%d", s.layerNum, s.totalLayers);
    lv_label_set_text(g_lbl_layer, layerBuf);

    // Speed
    lv_label_set_text(g_lbl_speed, speedLevelName(s.speedLevel));
    lv_obj_set_style_text_color(g_lbl_speed, speedLevelColor(s.speedLevel), LV_PART_MAIN);

    // Filament or WiFi
    if (s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS) {
        const AmsTray& t = s.ams.trays[s.ams.activeTray];
        if (t.present) {
            char buf[20];
            snprintf(buf, sizeof(buf), "* %s", t.type);
            lv_label_set_text(g_lbl_filament, buf);
            lv_obj_set_style_text_color(g_lbl_filament, c565(t.colorRgb565), LV_PART_MAIN);
            return;
        }
    } else if (s.ams.vtPresent && s.ams.activeTray == 254) {
        char buf[20];
        snprintf(buf, sizeof(buf), "* %s", s.ams.vtType);
        lv_label_set_text(g_lbl_filament, buf);
        lv_obj_set_style_text_color(g_lbl_filament, c565(s.ams.vtColorRgb565), LV_PART_MAIN);
        return;
    }
    char wifiBuf[16];
    snprintf(wifiBuf, sizeof(wifiBuf), "%ddBm", s.wifiSignal);
    lv_label_set_text(g_lbl_filament, wifiBuf);
    lv_obj_set_style_text_color(g_lbl_filament, c565(CLR_TEXT_DIM), LV_PART_MAIN);
}

// ---------------------------------------------------------------------------
//  Update bottom bar for IDLE / FINISHED
// ---------------------------------------------------------------------------
static void updateBottomIdle(const BambuState& s) {
    if (s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS) {
        const AmsTray& t = s.ams.trays[s.ams.activeTray];
        if (t.present) {
            char buf[20];
            snprintf(buf, sizeof(buf), "* %s", t.type);
            lv_label_set_text(g_lbl_filament, buf);
            lv_obj_set_style_text_color(g_lbl_filament, c565(t.colorRgb565), LV_PART_MAIN);
            return;
        }
    }
    char wifiBuf[24];
    snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: %ddBm", s.wifiSignal);
    lv_label_set_text(g_lbl_filament, wifiBuf);
    lv_obj_set_style_text_color(g_lbl_filament, c565(CLR_TEXT_DIM), LV_PART_MAIN);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void printerScreenInit() {
    buildPrinterScreen();
}

lv_obj_t* printerScreenGet() {
    return g_scr;
}

void printerScreenTransition() {
    smoothInited = false;
}

void printerScreenUpdate(const PrinterSlot& slot, ScreenState state) {
    const BambuState& s = slot.state;
    static ScreenState prevState = SCREEN_SPLASH;  // force initial configure

    // Reconfigure widget layout when state changes
    if (state != prevState) {
        configureForState(state);
        prevState = state;
    }

    tickSmooth(s, false);
    updateHeader(slot, state);

    // ── PRINTING ────────────────────────────────────────────────────────────
    if (state == SCREEN_PRINTING) {
        // Progress bar + arc
        lv_bar_set_value(g_bar_progress, s.progress, LV_ANIM_OFF);
        lv_arc_set_value(g_arc_progress, s.progress);

        char pctBuf[8], remBuf[12];
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", s.progress);
        if (s.remainingMinutes >= 60)
            snprintf(remBuf, sizeof(remBuf), "%dh%dm",
                     s.remainingMinutes / 60, s.remainingMinutes % 60);
        else
            snprintf(remBuf, sizeof(remBuf), "%dm", s.remainingMinutes);
        lv_label_set_text(g_lbl_prog_pct, pctBuf);
        lv_label_set_text(g_lbl_prog_rem, remBuf);

        // Nozzle arc + labels
        lv_arc_set_value(g_arc_nozzle, (int16_t)smoothNozzleTemp);
        char nozzBuf[8], nozzTgt[10];
        snprintf(nozzBuf, sizeof(nozzBuf), "%.0f", s.nozzleTemp);
        snprintf(nozzTgt, sizeof(nozzTgt), "/%.0f", s.nozzleTarget);
        lv_label_set_text(g_lbl_nozzle_v, nozzBuf);
        lv_label_set_text(g_lbl_nozzle_t, nozzTgt);
        lv_label_set_text(g_lbl_nozzle, nozzleLabel(s));

        // Bed arc + labels
        lv_arc_set_value(g_arc_bed, (int16_t)smoothBedTemp);
        char bedBuf[8], bedTgt[10];
        snprintf(bedBuf, sizeof(bedBuf), "%.0f", s.bedTemp);
        snprintf(bedTgt, sizeof(bedTgt), "/%.0f", s.bedTarget);
        lv_label_set_text(g_lbl_bed_v, bedBuf);
        lv_label_set_text(g_lbl_bed_t, bedTgt);

        // Fan arcs
        lv_arc_set_value(g_arc_pfan, (int16_t)smoothPartFan);
        char pfBuf[6]; snprintf(pfBuf, sizeof(pfBuf), "%d", s.coolingFanPct);
        lv_label_set_text(g_lbl_pfan_v, pfBuf);

        lv_arc_set_value(g_arc_afan, (int16_t)smoothAuxFan);
        char afBuf[6]; snprintf(afBuf, sizeof(afBuf), "%d", s.auxFanPct);
        lv_label_set_text(g_lbl_afan_v, afBuf);

        lv_arc_set_value(g_arc_cfan, (int16_t)smoothChamberFan);
        char cfBuf[6]; snprintf(cfBuf, sizeof(cfBuf), "%d", s.chamberFanPct);
        lv_label_set_text(g_lbl_cfan_v, cfBuf);

        // Fan arc color: dim if off
        lv_color_t pfColor = s.coolingFanPct == 0 ?
            c565(CLR_TEXT_DIM) : c565(dispSettings.partFan.arc);
        lv_color_t afColor = s.auxFanPct == 0 ?
            c565(CLR_TEXT_DIM) : c565(dispSettings.auxFan.arc);
        lv_color_t cfColor = s.chamberFanPct == 0 ?
            c565(CLR_TEXT_DIM) : c565(dispSettings.chamberFan.arc);
        lv_obj_set_style_arc_color(g_arc_pfan, pfColor, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(g_arc_afan, afColor, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(g_arc_cfan, cfColor, LV_PART_INDICATOR);

        // ETA line
        updateETA(s);

        // Bottom status bar
        updateBottomPrinting(s);

    // ── IDLE ────────────────────────────────────────────────────────────────
    } else if (state == SCREEN_IDLE) {
        if (!isAnyPrinterConfigured()) {
            // No printer configured
            lv_obj_set_style_text_color(g_lbl_state, c565(CLR_GREEN), LV_PART_MAIN);
            lv_label_set_text(g_lbl_state, "");
            lv_arc_set_value(g_arc_nozzle, 0);
            lv_arc_set_value(g_arc_bed, 0);
            lv_label_set_text(g_lbl_nozzle_v, "--");
            lv_label_set_text(g_lbl_nozzle_t, "");
            lv_label_set_text(g_lbl_bed_v, "--");
            lv_label_set_text(g_lbl_bed_t, "");
            char ipBuf[24];
            snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
            lv_label_set_text(g_lbl_filament, ipBuf);
        } else {
            // Nozzle arc
            lv_arc_set_value(g_arc_nozzle, (int16_t)smoothNozzleTemp);
            char nozzBuf[8], nozzTgt[10];
            snprintf(nozzBuf, sizeof(nozzBuf), "%.0f", s.nozzleTemp);
            snprintf(nozzTgt, sizeof(nozzTgt), "/%.0f", s.nozzleTarget);
            lv_label_set_text(g_lbl_nozzle_v, nozzBuf);
            lv_label_set_text(g_lbl_nozzle_t, nozzTgt);
            lv_label_set_text(g_lbl_nozzle, nozzleLabel(s));

            // Bed arc
            lv_arc_set_value(g_arc_bed, (int16_t)smoothBedTemp);
            char bedBuf[8], bedTgt[10];
            snprintf(bedBuf, sizeof(bedBuf), "%.0f", s.bedTemp);
            snprintf(bedTgt, sizeof(bedTgt), "/%.0f", s.bedTarget);
            lv_label_set_text(g_lbl_bed_v, bedBuf);
            lv_label_set_text(g_lbl_bed_t, bedTgt);

            updateBottomIdle(s);
        }

    // ── FINISHED ────────────────────────────────────────────────────────────
    } else if (state == SCREEN_FINISHED) {
        // Nozzle + bed (same as idle but different positions set in configureForState)
        lv_arc_set_value(g_arc_nozzle, (int16_t)smoothNozzleTemp);
        char nozzBuf[8], nozzTgt[10];
        snprintf(nozzBuf, sizeof(nozzBuf), "%.0f", s.nozzleTemp);
        snprintf(nozzTgt, sizeof(nozzTgt), "/%.0f", s.nozzleTarget);
        lv_label_set_text(g_lbl_nozzle_v, nozzBuf);
        lv_label_set_text(g_lbl_nozzle_t, nozzTgt);
        lv_label_set_text(g_lbl_nozzle, nozzleLabel(s));

        lv_arc_set_value(g_arc_bed, (int16_t)smoothBedTemp);
        char bedBuf[8], bedTgt[10];
        snprintf(bedBuf, sizeof(bedBuf), "%.0f", s.bedTemp);
        snprintf(bedTgt, sizeof(bedTgt), "/%.0f", s.bedTarget);
        lv_label_set_text(g_lbl_bed_v, bedBuf);
        lv_label_set_text(g_lbl_bed_t, bedTgt);

        lv_obj_set_style_text_color(g_lbl_eta, c565(CLR_GREEN), LV_PART_MAIN);
        lv_label_set_text(g_lbl_eta, "Print Complete!");

        if (s.subtaskName[0]) {
            char truncName[26];
            strncpy(truncName, s.subtaskName, 25);
            truncName[25] = '\0';
            lv_label_set_text(g_lbl_file, truncName);
        } else {
            lv_label_set_text(g_lbl_file, "");
        }

        updateBottomIdle(s);
    }
}

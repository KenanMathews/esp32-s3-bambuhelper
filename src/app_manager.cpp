#include "app_manager.h"
#include "config.h"
#include <LittleFS.h>
#include <Arduino.h>

#define APPS_DIR "/apps"

// ---------------------------------------------------------------------------
//  Static app registry
// ---------------------------------------------------------------------------
static AppInfo  s_apps[APP_MAX_INSTALLED];
static uint8_t  s_count = 0;

// ---------------------------------------------------------------------------
//  parseHeaders — read up to 256 bytes from file, extract comment headers
//  Supported: -- @name (required), -- @version, -- @color, -- @sdk_min
//  Global defaults apply for all optional fields when the tag is absent.
//  Returns false if @name is missing (app must be rejected).
// ---------------------------------------------------------------------------
static bool parseHeaders(const char* id, const uint8_t* buf, size_t len, AppInfo* out) {
    // Default values — all optional fields have a sensible fallback
    strlcpy(out->id,      id,    APP_ID_LEN);
    strlcpy(out->name,    "",    APP_NAME_LEN);    // empty until @name found
    strlcpy(out->version, "1.0", APP_VERSION_LEN); // default version
    out->color   = CLR_BTN;
    out->sdk_min = 1;                               // default: current SDK

    // Work on a null-terminated copy
    char tmp[257];
    size_t copyLen = (len < 256) ? len : 256;
    memcpy(tmp, buf, copyLen);
    tmp[copyLen] = '\0';

    // Helper lambda-style scan: find "-- @key" and return pointer to value
    auto findTag = [&](const char* tag) -> const char* {
        const char* p = strstr(tmp, tag);
        if (!p) return nullptr;
        p += strlen(tag);
        while (*p == ' ' || *p == '\t') p++;   // skip leading whitespace
        return p;
    };

    // Helper to copy a tag value into a fixed buffer, stopping at newline
    auto copyVal = [](const char* v, char* dst, uint8_t dstLen) {
        uint8_t i = 0;
        while (v[i] && v[i] != '\n' && v[i] != '\r' && i < dstLen - 1) {
            dst[i] = v[i]; i++;
        }
        dst[i] = '\0';
        return i;
    };

    // @name (mandatory)
    const char* v = findTag("-- @name");
    if (v) {
        char buf2[APP_NAME_LEN];
        if (copyVal(v, buf2, APP_NAME_LEN) > 0)
            strlcpy(out->name, buf2, APP_NAME_LEN);
    }
    if (out->name[0] == '\0') return false;   // @name missing — reject

    // @version
    v = findTag("-- @version");
    if (v) {
        char buf2[APP_VERSION_LEN];
        if (copyVal(v, buf2, APP_VERSION_LEN) > 0)
            strlcpy(out->version, buf2, APP_VERSION_LEN);
    }

    // @color
    v = findTag("-- @color");
    if (v) {
        char colBuf[12];
        if (copyVal(v, colBuf, sizeof(colBuf)) > 0)
            out->color = (uint16_t)strtol(colBuf, nullptr, 0 /* auto-detect 0x */);
    }

    // @sdk_min
    v = findTag("-- @sdk_min");
    if (v) {
        out->sdk_min = (uint8_t)atoi(v);
    }

    return true;
}

// ---------------------------------------------------------------------------
//  appManagerInit — mount LittleFS and scan /apps/*.lua
// ---------------------------------------------------------------------------
void appManagerInit() {
    s_count = 0;

    if (!LittleFS.begin(true)) {
        Serial.println("AppManager: LittleFS mount failed (formatted)");
        return;
    }

    // Ensure /apps directory exists
    if (!LittleFS.exists(APPS_DIR)) {
        LittleFS.mkdir(APPS_DIR);
        Serial.println("AppManager: created /apps dir");
        return;
    }

    File dir = LittleFS.open(APPS_DIR);
    if (!dir || !dir.isDirectory()) {
        Serial.println("AppManager: /apps not a directory");
        return;
    }

    File entry = dir.openNextFile();
    while (entry && s_count < APP_MAX_INSTALLED) {
        if (!entry.isDirectory()) {
            String fname = entry.name();  // just the filename, not full path
            if (fname.endsWith(".lua")) {
                // Derive id from filename (strip .lua)
                char id[APP_ID_LEN];
                String base = fname.substring(0, fname.length() - 4);
                strlcpy(id, base.c_str(), APP_ID_LEN);

                // Read first 256 bytes for header parsing
                uint8_t headBuf[256];
                size_t  readLen = entry.read(headBuf, sizeof(headBuf));

                if (!parseHeaders(id, headBuf, readLen, &s_apps[s_count])) {
                    Serial.printf("AppManager: skipped '%s' (missing @name)\n", id);
                } else {
                    Serial.printf("AppManager: found app '%s' v%s (%s)\n",
                                  s_apps[s_count].name, s_apps[s_count].version, id);
                    s_count++;
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    Serial.printf("AppManager: %d app(s) loaded\n", s_count);
}

// ---------------------------------------------------------------------------
//  appManagerCount
// ---------------------------------------------------------------------------
uint8_t appManagerCount() {
    return s_count;
}

// ---------------------------------------------------------------------------
//  appManagerGetApp
// ---------------------------------------------------------------------------
const AppInfo* appManagerGetApp(uint8_t index) {
    if (index >= s_count) return nullptr;
    return &s_apps[index];
}

// ---------------------------------------------------------------------------
//  appManagerGetAppById
// ---------------------------------------------------------------------------
const AppInfo* appManagerGetAppById(const char* id) {
    for (uint8_t i = 0; i < s_count; i++) {
        if (strcmp(s_apps[i].id, id) == 0) return &s_apps[i];
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
//  appManagerDelete — remove /apps/{id}.lua and rebuild registry
// ---------------------------------------------------------------------------
bool appManagerDelete(const char* id) {
    char path[APP_ID_LEN + 16];
    snprintf(path, sizeof(path), APPS_DIR "/%s.lua", id);

    if (!LittleFS.exists(path)) return false;
    LittleFS.remove(path);

    // Rebuild registry after deletion
    appManagerInit();
    return true;
}

// ---------------------------------------------------------------------------
//  appManagerLoadScript — load full file into PSRAM buffer
// ---------------------------------------------------------------------------
char* appManagerLoadScript(const char* id, size_t* outLen) {
    if (outLen) *outLen = 0;

    char path[APP_ID_LEN + 16];
    snprintf(path, sizeof(path), APPS_DIR "/%s.lua", id);

    File f = LittleFS.open(path, "r");
    if (!f) {
        Serial.printf("AppManager: cannot open %s\n", path);
        return nullptr;
    }

    size_t fsize = f.size();
    // Allocate in PSRAM; +1 for null terminator
    char* buf = (char*)heap_caps_malloc(fsize + 1, MALLOC_CAP_SPIRAM);
    if (!buf) {
        Serial.printf("AppManager: alloc failed for %zu bytes\n", fsize + 1);
        f.close();
        return nullptr;
    }

    size_t bytesRead = f.readBytes(buf, fsize);
    buf[bytesRead] = '\0';
    f.close();

    if (outLen) *outLen = bytesRead;
    return buf;
}

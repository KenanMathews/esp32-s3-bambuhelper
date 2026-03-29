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
//  Supported: -- @name, -- @color, -- @sdk_min
// ---------------------------------------------------------------------------
static void parseHeaders(const char* id, const uint8_t* buf, size_t len, AppInfo* out) {
    // Default values
    strlcpy(out->id, id, APP_ID_LEN);
    strlcpy(out->name, id, APP_NAME_LEN);   // fallback: use filename
    out->color   = CLR_BTN;
    out->sdk_min = 1;

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

    // @name
    const char* v = findTag("-- @name");
    if (v) {
        char nameBuf[APP_NAME_LEN];
        uint8_t i = 0;
        while (v[i] && v[i] != '\n' && v[i] != '\r' && i < APP_NAME_LEN - 1) {
            nameBuf[i] = v[i]; i++;
        }
        nameBuf[i] = '\0';
        if (i > 0) strlcpy(out->name, nameBuf, APP_NAME_LEN);
    }

    // @color
    v = findTag("-- @color");
    if (v) {
        char colBuf[12];
        uint8_t i = 0;
        while (v[i] && v[i] != '\n' && v[i] != '\r' && i < 11) {
            colBuf[i] = v[i]; i++;
        }
        colBuf[i] = '\0';
        if (i > 0) out->color = (uint16_t)strtol(colBuf, nullptr, 16 /* auto-detect 0x */);
    }

    // @sdk_min
    v = findTag("-- @sdk_min");
    if (v) {
        out->sdk_min = (uint8_t)atoi(v);
    }
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

                parseHeaders(id, headBuf, readLen, &s_apps[s_count]);
                Serial.printf("AppManager: found app '%s' (%s)\n",
                              s_apps[s_count].name, id);
                s_count++;
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

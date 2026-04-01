#ifndef APP_MANAGER_H
#define APP_MANAGER_H
#include <stdint.h>
#include <stddef.h>

#define APP_MAX_INSTALLED  8
#define APP_ID_LEN        32
#define APP_NAME_LEN      24
#define APP_VERSION_LEN   12

struct AppInfo {
    char     id[APP_ID_LEN];
    char     name[APP_NAME_LEN];
    char     version[APP_VERSION_LEN];   // @version tag, default "1.0"
    uint16_t color;
    uint8_t  sdk_min;
};

void            appManagerInit();
uint8_t         appManagerCount();
const AppInfo*  appManagerGetApp(uint8_t index);
const AppInfo*  appManagerGetAppById(const char* id);
bool            appManagerDelete(const char* id);
// Returns allocated buffer (caller must free), or nullptr on fail
char*           appManagerLoadScript(const char* id, size_t* outLen);

#endif // APP_MANAGER_H

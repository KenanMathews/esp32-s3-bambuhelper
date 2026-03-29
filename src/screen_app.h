#ifndef SCREEN_APP_H
#define SCREEN_APP_H
#include <lvgl.h>
#include <stdint.h>

void      appScreenInit();
lv_obj_t* appScreenGet();
// Called before loading an app — sets title, clears content area
void      appScreenPrepare(const char* appName);
// Called each loop while app is running
void      appScreenUpdate();

#endif // SCREEN_APP_H

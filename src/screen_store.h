#ifndef SCREEN_STORE_H
#define SCREEN_STORE_H
#include <lvgl.h>

// Called once at boot to create the LVGL screen object
void      storeScreenInit();
lv_obj_t* storeScreenGet();

// Called when the store tile is selected — kicks off HTTP fetch
void      storeScreenEnter();

// Must be called every loop() iteration while SCREEN_STORE is active
void      storeScreenUpdate();

#endif // SCREEN_STORE_H

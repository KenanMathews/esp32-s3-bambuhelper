#ifndef SCREEN_INFO_H
#define SCREEN_INFO_H

#include <lvgl.h>

void   infoScreenInit();
lv_obj_t* infoScreenGet();
void   infoScreenEnter();
void   infoScreenUpdate();

#endif // SCREEN_INFO_H

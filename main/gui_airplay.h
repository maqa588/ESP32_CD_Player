#ifndef __GUI_AIRPLAY_H_
#define __GUI_AIRPLAY_H_

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

void gui_airplay_init(void);
void gui_airplay_set_visible(bool visible);
void gui_airplay_update(void);
void gui_airplay_set_meter(int l, int r);

#endif // __GUI_AIRPLAY_H_

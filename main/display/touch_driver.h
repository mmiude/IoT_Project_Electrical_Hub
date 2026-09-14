#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include "lvgl.h"

void touch_init(void);
void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

#endif // TOUCH_DRIVER_H
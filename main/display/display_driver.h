#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "esp_lcd_panel_io.h"
#include "lvgl.h"

//screenrezz
#define LCD_H_RES  480
#define LCD_V_RES  320

esp_lcd_panel_io_handle_t display_init(void);
void display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

#endif // DISPLAY_DRIVER_H
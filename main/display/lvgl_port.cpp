#include "lvgl_port.h"
#include "display_driver.h"
#include "touch_driver.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lvgl.h"

#define LVGL_BUF_LINES 20

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(1);
}

void lvgl_port_init(void)
{
    display_init();
    touch_init();

    lv_init();

    static uint8_t *lvgl_draw_buf;
    lvgl_draw_buf = (uint8_t *)heap_caps_malloc(LCD_H_RES * LVGL_BUF_LINES * 2, MALLOC_CAP_DMA);

    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_flush_cb(disp, display_flush_cb);
    lv_display_set_buffers(disp, lvgl_draw_buf, NULL,
                            LCD_H_RES * LVGL_BUF_LINES * 2,
                            LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));
}
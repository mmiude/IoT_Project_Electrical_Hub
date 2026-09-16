#include "display_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "lvgl.h"

//#define PIN_SCLK 23
//#define PIN_MOSI 22
//#define PIN_MISO 21
//#define PIN_CS 1
//#define PIN_DC 8
//#define PIN_RST 14
//#define PIN_BL 15

// meh
static constexpr gpio_num_t PIN_SCLK = GPIO_NUM_23;
static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_22;
static constexpr gpio_num_t PIN_MISO = GPIO_NUM_21;
static constexpr gpio_num_t PIN_CS   = GPIO_NUM_1;
static constexpr gpio_num_t PIN_DC   = GPIO_NUM_8;
static constexpr gpio_num_t PIN_RST  = GPIO_NUM_14;
static constexpr gpio_num_t PIN_BL   = GPIO_NUM_15;

#define LCD_HOST SPI2_HOST
#define LVGL_BUF_LINES 20

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t data_len;
    uint16_t delay_ms;
} lcd_init_cmd_t;

static const lcd_init_cmd_t init_cmds[] = {
    {0x01, {0}, 0, 150},    // software reset
    {0x11, {0}, 0, 150},    // sleep out
    {0x3A, {0x66}, 1, 0},   // pixel format: 18-bit / 3 bytes per pixel
    {0x36, {0xE8}, 1, 0},   // memory access control (MX, BGR) (basically screen orientation/rotation)
    {0xB0, {0x00}, 1, 0},
    {0xB1, {0xA0}, 1, 0},
    {0xB4, {0x02}, 1, 0},
    {0xB6, {0x02, 0x02}, 2, 0},
    {0xE9, {0x00}, 1, 0},
    {0xF7, {0xA9, 0x51, 0x2C, 0x82}, 4, 0},
    {0x29, {0}, 0, 100},    // display ON
};

static esp_lcd_panel_io_handle_t io_handle = NULL;
static uint8_t *conv_buf = NULL;

static void lcd_send_cmd(uint8_t cmd, const uint8_t *data, size_t len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, cmd, data, len));
}

static void lcd_reset(void)
{
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
}

static void lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t caset[4] = { (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF) };
    uint8_t paset[4] = { (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF) };
    lcd_send_cmd(0x2A, caset, 4);
    lcd_send_cmd(0x2B, paset, 4);
}

void display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    lcd_set_window(area->x1, area->y1, area->x2, area->y2);

    uint16_t *src = (uint16_t *)px_map;
    int npixels = w * h;
    for (int i = 0; i < npixels; i++) {
        uint16_t px = src[i];
        uint8_t r5 = (px >> 11) & 0x1F;
        uint8_t g6 = (px >> 5) & 0x3F;
        uint8_t b5 = px & 0x1F;
        conv_buf[i * 3 + 0] = r5 << 3;
        conv_buf[i * 3 + 1] = g6 << 2;
        conv_buf[i * 3 + 2] = b5 << 3;
    }
    esp_lcd_panel_io_tx_color(io_handle, 0x2C, conv_buf, npixels * 3);
    lv_display_flush_ready(disp);
}

esp_lcd_panel_io_handle_t display_init(void)
{
    /*
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_RST) | (1ULL << PIN_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    */

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << PIN_RST) | (1ULL << PIN_BL);
    io_conf.mode = GPIO_MODE_OUTPUT;

    gpio_config(&io_conf);
    gpio_set_level(PIN_BL, 1);

    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = PIN_SCLK;
    buscfg.mosi_io_num = PIN_MOSI;
    buscfg.miso_io_num = PIN_MISO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = LCD_H_RES * LVGL_BUF_LINES * 3;
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num = PIN_DC;
    io_config.cs_gpio_num = PIN_CS;
    io_config.pclk_hz = 20 * 1000 * 1000;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    lcd_reset();
    for (size_t i = 0; i < sizeof(init_cmds) / sizeof(init_cmds[0]); i++) {
        lcd_send_cmd(init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_len);
        if (init_cmds[i].delay_ms) {
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }

    conv_buf = (uint8_t *)heap_caps_malloc(LCD_H_RES * LVGL_BUF_LINES * 3, MALLOC_CAP_DMA);
    return io_handle;
}
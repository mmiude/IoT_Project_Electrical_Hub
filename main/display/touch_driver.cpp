#include "touch_driver.h"
#include "display_driver.h"
#include "driver/i2c_master.h"
#include "lvgl.h"

//#define PIN_TOUCH_SCL 20
//#define PIN_TOUCH_SDA 19

static constexpr gpio_num_t PIN_TOUCH_SCL = GPIO_NUM_20;
static constexpr gpio_num_t PIN_TOUCH_SDA = GPIO_NUM_19;

#define GT911_ADDR 0x5D
#define GT911_REG_STATUS 0x814E
#define GT911_REG_POINT0 0x8150
#define GT911_REG_X_RES_L 0x8146

static i2c_master_dev_handle_t touch_dev = NULL;
static uint16_t touch_x_max = 320, touch_y_max = 480;
static bool touch_pressed = false;
static int touch_last_x = 0, touch_last_y = 0;

static esp_err_t gt911_read(uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t reg_buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    return i2c_master_transmit_receive(touch_dev, reg_buf, 2, data, len, 100);
}

static esp_err_t gt911_write_u8(uint16_t reg, uint8_t val)
{
    uint8_t buf[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val };
    return i2c_master_transmit(touch_dev, buf, 3, 100);
}

static void touch_to_screen(uint16_t tx, uint16_t ty, int *sx, int *sy)
{
    *sx = ((int)(touch_y_max - ty) * (LCD_H_RES - 1)) / touch_y_max;
    *sy = ((int)tx * (LCD_V_RES - 1)) / touch_x_max;
}

void touch_init(void)
{
    i2c_master_bus_config_t touch_bus_config = {};
    touch_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    touch_bus_config.i2c_port = I2C_NUM_0;
    touch_bus_config.scl_io_num = PIN_TOUCH_SCL;
    touch_bus_config.sda_io_num = PIN_TOUCH_SDA;
    touch_bus_config.glitch_ignore_cnt = 7;
    touch_bus_config.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t touch_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&touch_bus_config, &touch_bus));

    i2c_device_config_t touch_dev_config = {};
    touch_dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    touch_dev_config.device_address = GT911_ADDR;
    touch_dev_config.scl_speed_hz = 400000;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(touch_bus, &touch_dev_config, &touch_dev));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(touch_bus, &touch_dev_config, &touch_dev));

    uint8_t buf[4];
    if (gt911_read(GT911_REG_X_RES_L, buf, 4) == ESP_OK) {
        touch_x_max = buf[0] | (buf[1] << 8);
        touch_y_max = buf[2] | (buf[3] << 8);
    }
}

void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint8_t status = 0;
    if (gt911_read(GT911_REG_STATUS, &status, 1) == ESP_OK) {
        uint8_t touch_ready = status & 0x80;
        uint8_t num_points = status & 0x0F;
        if (touch_ready) {
            if (num_points > 0 && num_points <= 5) {
                uint8_t buf[8];
                if (gt911_read(GT911_REG_POINT0, buf, 8) == ESP_OK) {
                    uint16_t x = buf[0] | (buf[1] << 8);
                    uint16_t y = buf[2] | (buf[3] << 8);
                    touch_to_screen(x, y, &touch_last_x, &touch_last_y);
                    touch_pressed = true;
                }
            } else {
                touch_pressed = false;
            }
            gt911_write_u8(GT911_REG_STATUS, 0x00);
        }
    }
    data->point.x = touch_last_x;
    data->point.y = touch_last_y;
    data->state = touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
#include "rtc.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "rtc";

#define I2C_MASTER_SCL_IO    GPIO_NUM_18
#define I2C_MASTER_SDA_IO    GPIO_NUM_17
#define I2C_MASTER_FREQ_HZ   100000
#define DS3231_ADDR          0x68

static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_master_dev_handle_t s_dev_handle = NULL;

static uint8_t bcd_to_dec(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0F);
}

static uint8_t dec_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

esp_err_t ds3231_init(void)
{
    ESP_LOGI(TAG, "初始化 RTC");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C 总线创建失败: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(s_bus_handle, &dev_config, &s_dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C 设备添加失败: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t data[2] = {0x0E, 0x00};
    err = i2c_master_transmit(s_dev_handle, data, 2, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RTC 控制寄存器配置失败: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "RTC 初始化完成");
    return ESP_OK;
}

esp_err_t ds3231_get_time(struct tm *timeinfo)
{
    uint8_t reg = 0x00;
    uint8_t data[7];

    esp_err_t err = i2c_master_transmit_receive(s_dev_handle, &reg, 1, data, 7, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "读取 RTC 时间失败: %s", esp_err_to_name(err));
        return err;
    }

    timeinfo->tm_sec = bcd_to_dec(data[0] & 0x7F);
    timeinfo->tm_min = bcd_to_dec(data[1] & 0x7F);

    if (data[2] & 0x40) {
        timeinfo->tm_hour = bcd_to_dec(data[2] & 0x1F);
        if (data[2] & 0x20) {
            timeinfo->tm_hour += 12;
        }
    } else {
        timeinfo->tm_hour = bcd_to_dec(data[2] & 0x3F);
    }

    timeinfo->tm_mday = bcd_to_dec(data[4] & 0x3F);
    timeinfo->tm_mon = bcd_to_dec(data[5] & 0x1F) - 1;
    timeinfo->tm_year = bcd_to_dec(data[6]) + 100;

    return ESP_OK;
}

esp_err_t ds3231_set_time(const struct tm *timeinfo)
{
    uint8_t data[8];
    data[0] = 0x00;
    data[1] = dec_to_bcd(timeinfo->tm_sec);
    data[2] = dec_to_bcd(timeinfo->tm_min);
    data[3] = dec_to_bcd(timeinfo->tm_hour);
    data[4] = dec_to_bcd(timeinfo->tm_wday + 1);
    data[5] = dec_to_bcd(timeinfo->tm_mday);
    data[6] = dec_to_bcd(timeinfo->tm_mon + 1);
    data[7] = dec_to_bcd(timeinfo->tm_year - 100);

    esp_err_t err = i2c_master_transmit(s_dev_handle, data, 8, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置 RTC 时间失败: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

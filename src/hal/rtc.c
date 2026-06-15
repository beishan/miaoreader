#include "rtc.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "rtc";

#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_SCL_IO    GPIO_NUM_18
#define I2C_MASTER_SDA_IO    GPIO_NUM_17
#define I2C_MASTER_FREQ_HZ   100000
#define DS3231_ADDR          0x68

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
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C 参数配置失败");
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C 驱动安装失败");
        return err;
    }
    
    uint8_t data[2] = {0x0E, 0x00};
    err = i2c_master_write_to_device(I2C_MASTER_NUM, DS3231_ADDR, data, 2, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RTC 控制寄存器配置失败");
    }
    
    ESP_LOGI(TAG, "RTC 初始化完成");
    return ESP_OK;
}

esp_err_t ds3231_get_time(struct tm *timeinfo)
{
    uint8_t reg = 0x00;
    uint8_t data[7];
    
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, DS3231_ADDR, &reg, 1, data, 7, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "读取 RTC 时间失败");
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
    
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, DS3231_ADDR, data, 8, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置 RTC 时间失败");
        return err;
    }
    
    return ESP_OK;
}

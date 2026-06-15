#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

#define ADC_CHANNEL      ADC_CHANNEL_0
#define ADC_ATTEN        ADC_ATTEN_DB_12
#define ADC_BITWIDTH     ADC_BITWIDTH_12
#define VOLTAGE_DIVIDER  2.0f
#define BATTERY_FULL_MV  4200
#define BATTERY_EMPTY_MV 3300
#define LOW_BATTERY_PCT  5
#define WARNING_BATTERY_PCT 15

static adc_oneshot_unit_handle_t adc_handle = NULL;
static int adc_raw = 0;

static const uint16_t voltage_to_soc[] = {
    4200, 100,
    4060, 90,
    3980, 80,
    3920, 70,
    3870, 60,
    3820, 50,
    3780, 40,
    3730, 30,
    3680, 20,
    3500, 10,
    3300, 0,
};

static uint16_t mv_to_soc(uint16_t mv)
{
    if (mv >= voltage_to_soc[0]) return 100;
    if (mv <= voltage_to_soc[20]) return 0;
    
    for (int i = 0; i < 20; i += 2) {
        if (mv >= voltage_to_soc[i + 2]) {
            uint16_t v_high = voltage_to_soc[i];
            uint16_t v_low = voltage_to_soc[i + 2];
            uint16_t soc_high = voltage_to_soc[i + 1];
            uint16_t soc_low = voltage_to_soc[i + 3];
            
            return soc_low + (uint16_t)((uint32_t)(mv - v_low) * (soc_high - soc_low) / (v_high - v_low));
        }
    }
    return 0;
}

esp_err_t battery_init(void)
{
    ESP_LOGI(TAG, "初始化电池检测");
    
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    
    esp_err_t err = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC 单元初始化失败");
        return err;
    }
    
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    
    err = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC 通道配置失败");
        return err;
    }
    
    ESP_LOGI(TAG, "电池检测初始化完成");
    return ESP_OK;
}

uint8_t battery_get_percent(void)
{
    if (!adc_handle) return 0;
    
    int raw_sum = 0;
    int valid_count = 0;
    
    for (int i = 0; i < 16; i++) {
        int raw = 0;
        if (adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw) == ESP_OK) {
            raw_sum += raw;
            valid_count++;
        }
    }
    
    if (valid_count == 0) return 0;
    
    adc_raw = raw_sum / valid_count;
    
    uint16_t voltage_mv = (uint16_t)(adc_raw * 3300 / 4095 * VOLTAGE_DIVIDER);
    
    return (uint8_t)mv_to_soc(voltage_mv);
}

bool battery_is_charging(void)
{
    return false;
}

bool battery_is_low(void)
{
    return battery_get_percent() < LOW_BATTERY_PCT;
}

bool battery_is_warning(void)
{
    return battery_get_percent() < WARNING_BATTERY_PCT;
}

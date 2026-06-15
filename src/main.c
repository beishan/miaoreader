#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "妙阅 电子书阅读器启动");
    
    // NVS 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // TODO: HAL 初始化
    // TODO: 加载配置
    // TODO: 启动 UI 状态机
    // TODO: 启动网页服务

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

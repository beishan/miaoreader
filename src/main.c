#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "engine/config.h"
#include "engine/event_bus.h"
#include "hal/epd.h"
#include "ui/page_mgr.h"
#include "ui/widget.h"
#include "ui/status_bar.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "妙阅 电子书阅读器启动");

    /* NVS 初始化 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 配置 + 事件总线初始化 */
    ESP_ERROR_CHECK(config_init());
    ESP_ERROR_CHECK(event_bus_init());

    /* 硬件初始化 */
    ESP_ERROR_CHECK(epd_init());
    widget_init();

    /* UI 初始化 */
    ESP_ERROR_CHECK(page_mgr_init());
    status_bar_init();

    /* 显示启动画面（状态栏 + 待办：主页） */
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();
    epd_display_partial();

    ESP_LOGI(TAG, "初始化完成，进入主循环");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

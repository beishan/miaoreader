#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "engine/config.h"
#include "engine/event_bus.h"
#include "engine/typesetter.h"
#include "engine/power_mgr.h"
#include "hal/epd.h"
#include "hal/keys.h"
#include "hal/battery.h"
#include "hal/rtc.h"
#include "hal/sd_card.h"
#include "hal/wifi_mgr.h"
#include "net/sntp.h"
#include "net/weather.h"
#include "net/web_server.h"
#include "net/ota.h"
#include "ui/page_mgr.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_home.h"
#include "ui/page_bookshelf.h"
#include "ui/page_reader.h"
#include "ui/page_settings.h"
#include "ui/page_sleep.h"
#include "ui/page_boot.h"
#include "ui/page_menu.h"
#include "ui/page_jump.h"
#include "engine/bookmark.h"
#include "engine/book_meta.h"
#include "engine/reading_stats.h"

static const char *TAG = "main";

/* WiFi 连接任务 */
static void wifi_connect_task(void *arg)
{
    SysConfig *cfg = (SysConfig *)arg;

    /* 等待系统初始化完成 */
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (!cfg->initialized || cfg->ssid[0] == '\0') {
        ESP_LOGI(TAG, "WiFi 未配置，启动 AP 配网模式");
        wifi_mgr_start_ap();
        web_server_start();
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "尝试连接 WiFi: %s", cfg->ssid);
    esp_err_t err = wifi_mgr_connect_sta(cfg->ssid, cfg->password);

    /* 等待连接结果（最多 15 秒） */
    int timeout = 30;
    while (timeout > 0 && !wifi_mgr_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
        timeout--;
    }

    if (wifi_mgr_is_connected()) {
        char ip[16];
        wifi_mgr_get_ip(ip, sizeof(ip));
        ESP_LOGI(TAG, "WiFi 已连接: %s", ip);

        /* 同步时间 */
        ESP_LOGI(TAG, "同步 SNTP 时间...");
        sntp_sync_init();
        sntp_force_sync();

        /* 刷新天气 */
        ESP_LOGI(TAG, "刷新天气数据...");
        weather_init();
        weather_refresh();

        /* 启动网页服务器 */
        ESP_LOGI(TAG, "启动网页服务器...");
        web_server_init();
        web_server_start();
    } else {
        ESP_LOGW(TAG, "WiFi 连接超时，启动 AP 配网模式");
        wifi_mgr_start_ap();
        web_server_init();
        web_server_start();
    }

    vTaskDelete(NULL);
}

/* 按键分发：组合键处理 + 页面按键 */
static void main_key_handler(KeyId key, KeyEvent event)
{
    /* 组合键：PREV + NEXT → 全刷清洁 */
    if (event == KEY_EVT_COMBO) {
        ESP_LOGI(TAG, "组合键触发：全刷清洁");
        epd_clear(EPD_COLOR_BLACK);
        epd_display_full();
        vTaskDelay(pdMS_TO_TICKS(2000));
        epd_clear(EPD_COLOR_WHITE);
        epd_display_full();
        return;
    }

    /* 其他按键：分发给当前页面 */
    page_mgr_handle_key(key, event);
}

/* 事件总线处理：重置空闲计时器 */
static void key_event_handler(EventType type, void *data)
{
    if (type == EVT_KEY) {
        power_mgr_reset_idle();
    }
}

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

    /* Task WDT 初始化（超时 30 秒） */
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = 0,  /* 不监控 idle 任务 */
        .trigger_panic = true,
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdt_config));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));  /* 注册当前任务 (app_main) */
    ESP_LOGI(TAG, "Task WDT 已启用（超时 30 秒）");

    /* 配置 + 事件总线初始化 */
    ESP_ERROR_CHECK(config_init());
    ESP_ERROR_CHECK(event_bus_init());

    /* 硬件初始化 */
    ESP_ERROR_CHECK(epd_init());
    ESP_ERROR_CHECK(ds3231_init());
    ESP_ERROR_CHECK(battery_init());
    ESP_ERROR_CHECK(sd_card_init());
    widget_init();

    /* UI 初始化 */
    ESP_ERROR_CHECK(page_mgr_init());
    status_bar_init();

    /* 排版器初始化（注册默认字体路径） */
    TypesetterConfig tcfg = {
        .fontId = 0, .fontSize = 20, .lineSpacing = 15,
        .charSpacing = 0, .margin = 12,
        .pageWidth = 376, .pageHeight = 262,
    };
    typesetter_register_font(0, "/sdcard/fonts/SourceHanSerif.ttf");
    typesetter_register_font(1, "/sdcard/fonts/SourceHanSans.ttf");
    typesetter_register_font(2, "/sdcard/fonts/LXGWWenKai.ttf");
    typesetter_init(&tcfg);

    /* WiFi 管理器初始化 */
    ESP_ERROR_CHECK(wifi_mgr_init());

    /* OTA 初始化 */
    ESP_ERROR_CHECK(ota_init());

    /* 书签初始化 */
    ESP_ERROR_CHECK(bookmark_init());

    /* 书籍元数据初始化 */
    ESP_ERROR_CHECK(book_meta_init());

    /* 阅读统计初始化 */
    ESP_ERROR_CHECK(reading_stats_init());

    /* 电源管理器初始化 */
    ESP_ERROR_CHECK(power_mgr_init());

    /* 注册页面 */
    page_mgr_register(&page_boot_vtbl);
    page_mgr_register(&page_home_vtbl);
    page_mgr_register(&page_bookshelf_vtbl);
    page_mgr_register(&page_reader_vtbl);
    page_mgr_register(&page_settings_vtbl);
    page_mgr_register(&page_sleep_vtbl);
    page_mgr_register(&page_menu_vtbl);
    page_mgr_register(&page_jump_vtbl);

    /* 初始化按键：组合键处理 + 页面分发 */
    keys_init(main_key_handler);

    /* 订阅按键事件，重置空闲计时器 */
    event_bus_subscribe(EVT_KEY, key_event_handler);

    /* 加载系统配置 */
    SysConfig sys_cfg;
    config_load_sys(&sys_cfg);

    /* 启动 WiFi 连接任务 */
    xTaskCreate(wifi_connect_task, "wifi_connect", 4096, &sys_cfg, 5, NULL);

    /* 根据初始化状态选择页面 */
    if (!sys_cfg.initialized) {
        page_mgr_switch(PAGE_BOOT);
    } else if (page_reader_try_restore()) {
        /* 崩溃恢复：有上次阅读记录，直接进入阅读器 */
        ESP_LOGI(TAG, "恢复上次阅读状态");
        page_mgr_switch(PAGE_READER);
    } else {
        page_mgr_switch(PAGE_HOME);
    }

    ESP_LOGI(TAG, "初始化完成，进入主循环");

    while (1) {
        esp_task_wdt_reset();  /* 喂狗 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

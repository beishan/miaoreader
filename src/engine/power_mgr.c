/**
 * @file power_mgr.c
 * @brief 电源管理模块：空闲休眠、深度睡眠、低电量保护
 */
#include "power_mgr.h"
#include "engine/config.h"
#include "engine/event_bus.h"
#include "engine/types.h"
#include "hal/battery.h"
#include "hal/epd.h"
#include "ui/page_mgr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "power_mgr";

/* PWR 键 GPIO（用于深度睡眠唤醒） */
#define PWR_GPIO         GPIO_NUM_0
#define PWR_GPIO_MASK     (1ULL << PWR_GPIO)

/* 空闲计时器 */
static esp_timer_handle_t s_idle_timer = NULL;
static uint32_t s_idle_timeout_sec = 300;  /* 默认 5 分钟 */
static bool s_sleeping = false;

/* 电池监控任务 */
#define BATTERY_CHECK_INTERVAL_MS  (5 * 60 * 1000)  /* 5 分钟 */

/* 空闲计时器回调 */
static void idle_timer_cb(void *arg)
{
    if (s_sleeping) return;

    ESP_LOGI(TAG, "空闲超时，进入休眠");
    power_mgr_enter_sleep();
}

/* 电池监控任务 */
static void battery_monitor_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(BATTERY_CHECK_INTERVAL_MS));

        uint8_t pct = battery_get_percent();
        bool low = battery_is_low();

        /* 发布电池事件 */
        event_bus_publish(EVT_BATTERY, &pct);

        ESP_LOGI(TAG, "电池电量: %u%% %s", pct, low ? "[低电量]" : "");

        /* 低电量强制休眠 */
        if (low) {
            ESP_LOGW(TAG, "电量不足，强制进入深度睡眠");
            power_mgr_deep_sleep();
        }
    }
}

esp_err_t power_mgr_init(void)
{
    ESP_LOGI(TAG, "初始化电源管理器");

    /* 加载休眠超时配置 */
    SysConfig cfg;
    config_load_sys(&cfg);
    if (cfg.sleepTimeoutSec > 0) {
        s_idle_timeout_sec = cfg.sleepTimeoutSec;
    }

    /* 创建空闲计时器 */
    esp_timer_create_args_t timer_cfg = {
        .callback = idle_timer_cb,
        .name = "idle_timer",
    };
    esp_err_t err = esp_timer_create(&timer_cfg, &s_idle_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "空闲计时器创建失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 启动空闲计时器 */
    err = esp_timer_start_once(s_idle_timer, s_idle_timeout_sec * 1000000ULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "空闲计时器启动失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 启动电池监控任务 */
    xTaskCreate(battery_monitor_task, "battery_mon", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "电源管理器初始化完成，空闲超时: %lu 秒", (unsigned long)s_idle_timeout_sec);
    return ESP_OK;
}

void power_mgr_reset_idle(void)
{
    if (s_sleeping || !s_idle_timer) return;

    /* 重启空闲计时器 */
    esp_timer_stop(s_idle_timer);
    esp_timer_start_once(s_idle_timer, s_idle_timeout_sec * 1000000ULL);
}

void power_mgr_enter_sleep(void)
{
    if (s_sleeping) return;
    s_sleeping = true;

    ESP_LOGI(TAG, "进入休眠页面");

    /* 发布休眠事件 */
    event_bus_publish(EVT_SLEEP, NULL);

    /* 切换到休眠页面 */
    page_mgr_switch(PAGE_SLEEP);

    /* 等待 E-Ink 刷新完成 */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* 进入深度睡眠 */
    power_mgr_deep_sleep();
}

void power_mgr_deep_sleep(void)
{
    ESP_LOGI(TAG, "进入深度睡眠");

    /* 配置 PWR 键唤醒 */
    esp_sleep_enable_ext0_wakeup(PWR_GPIO, 0);  /* 低电平唤醒 */

    /* 关闭外设 */
    epd_sleep();

    /* 停止计时器 */
    if (s_idle_timer) {
        esp_timer_stop(s_idle_timer);
    }

    ESP_LOGI(TAG, "深度睡眠启动，按 PWR 键唤醒");

    /* 进入深度睡眠 */
    esp_deep_sleep_start();
}

bool power_mgr_is_low_battery(void)
{
    return battery_is_low();
}

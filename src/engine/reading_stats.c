/**
 * @file reading_stats.c
 * @brief 阅读统计模块：今日阅读时间、累计阅读时间
 */
#include "reading_stats.h"
#include "hal/rtc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "reading_stats";

static const char *NVS_NAMESPACE = "read_stats";
static const char *NVS_KEY_TODAY = "today_sec";
static const char *NVS_KEY_TOTAL = "total_sec";
static const char *NVS_KEY_DATE = "today_date";

static uint32_t s_today_seconds = 0;
static uint32_t s_total_seconds = 0;
static uint32_t s_session_start = 0;
static bool s_in_session = false;
static int s_last_day = -1;
static bool s_initialized = false;

/* 获取当前时间戳（秒） */
static uint32_t get_timestamp(void)
{
    struct tm t;
    if (ds3231_get_time(&t) == ESP_OK) {
        return (uint32_t)mktime(&t);
    }
    return 0;
}

/* 获取当前日期（天数，用于判断日期变化） */
static int get_current_day(void)
{
    struct tm t;
    if (ds3231_get_time(&t) == ESP_OK) {
        return t.tm_year * 366 + t.tm_yday;
    }
    return -1;
}

/* 从 NVS 加载统计数据 */
static void load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return;

    nvs_get_u32(handle, NVS_KEY_TODAY, &s_today_seconds);
    nvs_get_u32(handle, NVS_KEY_TOTAL, &s_total_seconds);

    /* 读取上次保存的日期 */
    uint32_t saved_date = 0;
    if (nvs_get_u32(handle, NVS_KEY_DATE, &saved_date) == ESP_OK) {
        s_last_day = (int)saved_date;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "从 NVS 加载: 今日=%lu秒, 累计=%lu秒", (unsigned long)s_today_seconds, (unsigned long)s_total_seconds);
}

/* 保存统计数据到 NVS */
static void save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return;

    nvs_set_u32(handle, NVS_KEY_TODAY, s_today_seconds);
    nvs_set_u32(handle, NVS_KEY_TOTAL, s_total_seconds);
    nvs_set_u32(handle, NVS_KEY_DATE, (uint32_t)get_current_day());
    nvs_commit(handle);
    nvs_close(handle);
}

esp_err_t reading_stats_init(void)
{
    if (s_initialized) return ESP_OK;

    ESP_LOGI(TAG, "初始化阅读统计模块");
    load_from_nvs();

    /* 检查日期是否变化 */
    reading_stats_check_day_reset();

    s_initialized = true;
    return ESP_OK;
}

void reading_stats_start_session(void)
{
    if (s_in_session) return;

    s_session_start = get_timestamp();
    s_in_session = true;
    ESP_LOGI(TAG, "开始阅读计时");
}

void reading_stats_end_session(void)
{
    if (!s_in_session) return;

    uint32_t now = get_timestamp();
    if (now > s_session_start) {
        uint32_t duration = now - s_session_start;
        s_today_seconds += duration;
        s_total_seconds += duration;
        save_to_nvs();
        ESP_LOGI(TAG, "结束阅读计时: +%lu秒 (今日=%lu秒)", (unsigned long)duration, (unsigned long)s_today_seconds);
    }

    s_in_session = false;
    s_session_start = 0;
}

uint32_t reading_stats_get_today_seconds(void)
{
    /* 如果正在阅读，加上当前会话时间 */
    uint32_t extra = 0;
    if (s_in_session) {
        uint32_t now = get_timestamp();
        if (now > s_session_start) {
            extra = now - s_session_start;
        }
    }
    return s_today_seconds + extra;
}

uint32_t reading_stats_get_today_minutes(void)
{
    return reading_stats_get_today_seconds() / 60;
}

uint32_t reading_stats_get_total_seconds(void)
{
    uint32_t extra = 0;
    if (s_in_session) {
        uint32_t now = get_timestamp();
        if (now > s_session_start) {
            extra = now - s_session_start;
        }
    }
    return s_total_seconds + extra;
}

uint32_t reading_stats_get_total_hours(void)
{
    return reading_stats_get_total_seconds() / 3600;
}

void reading_stats_check_day_reset(void)
{
    int current_day = get_current_day();
    if (current_day < 0) return;

    if (s_last_day >= 0 && s_last_day != current_day) {
        ESP_LOGI(TAG, "日期变化，重置今日统计");
        s_today_seconds = 0;
        save_to_nvs();
    }

    s_last_day = current_day;
}

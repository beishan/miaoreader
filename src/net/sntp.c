/**
 * @file sntp.c
 * @brief SNTP 时间同步模块
 */
#include "sntp.h"
#include "hal/rtc.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "sntp";

static bool s_initialized = false;
static bool s_synced = false;
static uint32_t s_last_sync_time = 0;

/* SNTP 同步完成回调 */
static void sntp_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP 同步完成: %ld", (long)tv->tv_sec);
    s_synced = true;
    s_last_sync_time = (uint32_t)tv->tv_sec;

    /* 同步到 RTC */
    struct tm timeinfo;
    localtime_r(&tv->tv_sec, &timeinfo);
    esp_err_t err = ds3231_set_time(&timeinfo);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "RTC 时间已更新: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        ESP_LOGW(TAG, "RTC 时间更新失败: %s", esp_err_to_name(err));
    }
}

esp_err_t sntp_sync_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "SNTP 已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化 SNTP");

    /* 设置时区为东八区（中国标准时间） */
    setenv("TZ", "CST-8", 1);
    tzset();

    /* 配置 SNTP */
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "cn.ntp.org.cn");
    esp_sntp_setservername(2, "pool.ntp.org");

    /* 设置同步回调 */
    esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);

    /* 初始化 SNTP */
    esp_sntp_init();

    s_initialized = true;
    ESP_LOGI(TAG, "SNTP 初始化完成，服务器: ntp.aliyun.com");
    return ESP_OK;
}

esp_err_t sntp_force_sync(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "SNTP 未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "强制 SNTP 同步...");

    /* 等待同步完成（最多 10 秒） */
    int retry = 0;
    const int max_retry = 20;
    while (!s_synced && retry < max_retry) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }

    if (s_synced) {
        ESP_LOGI(TAG, "SNTP 同步成功");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "SNTP 同步超时");
        return ESP_ERR_TIMEOUT;
    }
}

bool sntp_is_synced(void)
{
    return s_synced;
}

uint32_t sntp_get_last_sync_time(void)
{
    return s_last_sync_time;
}

/**
 * @file sntp.h
 * @brief SNTP 时间同步模块
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SNTP 时间同步
 *
 * 配置 SNTP 服务器和工作模式。
 * 需要在 WiFi 连接成功后调用 sntp_force_sync() 触发同步。
 */
esp_err_t sntp_sync_init(void);

/**
 * @brief 强制同步时间
 *
 * 触发一次 SNTP 时间同步。
 * 同步成功后会自动更新 RTC。
 *
 * @return ESP_OK 同步成功
 */
esp_err_t sntp_force_sync(void);

/**
 * @brief 检查时间是否已同步
 */
bool sntp_is_synced(void);

/**
 * @brief 获取上次同步的时间戳
 *
 * @return Unix 时间戳，0 表示未同步
 */
uint32_t sntp_get_last_sync_time(void);

#ifdef __cplusplus
}
#endif

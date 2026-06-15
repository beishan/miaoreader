/**
 * @file reading_stats.h
 * @brief 阅读统计模块：今日阅读时间、累计阅读时间
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化阅读统计模块
 */
esp_err_t reading_stats_init(void);

/**
 * @brief 开始阅读计时
 *
 * 进入阅读器时调用。
 */
void reading_stats_start_session(void);

/**
 * @brief 结束阅读计时
 *
 * 退出阅读器时调用，自动累加时间。
 */
void reading_stats_end_session(void);

/**
 * @brief 获取今日阅读时间（秒）
 */
uint32_t reading_stats_get_today_seconds(void);

/**
 * @brief 获取今日阅读时间（分钟）
 */
uint32_t reading_stats_get_today_minutes(void);

/**
 * @brief 获取累计阅读时间（秒）
 */
uint32_t reading_stats_get_total_seconds(void);

/**
 * @brief 获取累计阅读时间（小时）
 */
uint32_t reading_stats_get_total_hours(void);

/**
 * @brief 检查日期是否变化，如果变化则重置今日统计
 */
void reading_stats_check_day_reset(void);

#ifdef __cplusplus
}
#endif

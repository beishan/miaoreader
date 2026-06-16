/**
 * @file mock_rtc.h
 * @brief Mock RTC 模块 - 模拟 DS3231 实时时钟
 */
#pragma once

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 mock RTC
 */
void mock_rtc_init(void);

/**
 * @brief 获取当前时间
 * @param tm 输出时间结构体
 */
void mock_rtc_get_time(struct tm *tm);

/**
 * @brief 获取时间字符串 (HH:MM)
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_rtc_get_time_str(char *buf, int size);

/**
 * @brief 获取日期字符串 (YYYY-MM-DD)
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_rtc_get_date_str(char *buf, int size);

/**
 * @brief 获取星期几 (中文)
 * @return 星期几的中文字符串
 */
const char* mock_rtc_get_weekday_cn(void);

#ifdef __cplusplus
}
#endif

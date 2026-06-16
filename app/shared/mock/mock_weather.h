/**
 * @file mock_weather.h
 * @brief Mock 天气模块 - 模拟和风天气 API
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 天气数据结构
 */
typedef struct {
    char city[32];          /* 城市名 */
    char condition[32];     /* 天气状况 */
    int temp_now;           /* 当前温度 */
    int temp_low;           /* 最低温度 */
    int temp_high;          /* 最高温度 */
    int humidity;           /* 湿度 (%) */
    char aqi[16];           /* 空气质量指数 */
    char alert[64];         /* 预警信息 */
} MockWeatherData;

/**
 * @brief 初始化 mock 天气
 */
void mock_weather_init(void);

/**
 * @brief 获取当前天气
 * @param data 输出天气数据
 */
void mock_weather_get_current(MockWeatherData *data);

/**
 * @brief 获取天气摘要字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_weather_get_summary(char *buf, int size);

/**
 * @brief 获取温度字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_weather_get_temp_str(char *buf, int size);

/**
 * @brief 获取详细信息字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_weather_get_detail_str(char *buf, int size);

#ifdef __cplusplus
}
#endif

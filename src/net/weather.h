/**
 * @file weather.h
 * @brief 天气服务模块：和风天气 API 集成
 */
#pragma once

#include "engine/types.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化天气服务
 *
 * 从 NVS 加载缓存的天气数据。
 * 需要在 WiFi 连接后调用 weather_refresh() 获取最新数据。
 */
esp_err_t weather_init(void);

/**
 * @brief 刷新天气数据
 *
 * 调用和风天气 API 获取最新天气数据。
 * 需要已连接 WiFi。
 *
 * @return ESP_OK 获取成功
 */
esp_err_t weather_refresh(void);

/**
 * @brief 获取当前天气数据
 *
 * @param data 输出天气数据
 * @return ESP_OK 数据有效
 */
esp_err_t weather_get_current(WeatherData *data);

/**
 * @brief 获取天气预报
 *
 * @param data 输出预报数据
 * @param days 天数（最多 3 天）
 * @return ESP_OK 数据有效
 */
esp_err_t weather_get_forecast(WeatherForecast *data, int days);

/**
 * @brief 检查天气数据是否过期
 *
 * @return true 数据过期（超过 30 分钟）
 */
bool weather_is_stale(void);

/**
 * @brief 配置天气 API
 *
 * @param api_key 和风天气 API Key
 * @param city 城市名或 Location ID
 * @return ESP_OK 配置成功
 */
esp_err_t weather_config(const char *api_key, const char *city);

#ifdef __cplusplus
}
#endif

/**
 * @file mock_wifi.h
 * @brief Mock WiFi 模块 - 模拟 WiFi 连接状态
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 mock WiFi
 */
void mock_wifi_init(void);

/**
 * @brief 检查是否已连接
 * @return true 已连接，false 未连接
 */
bool mock_wifi_is_connected(void);

/**
 * @brief 模拟连接 WiFi
 * @param ssid WiFi 名称
 */
void mock_wifi_connect(const char *ssid);

/**
 * @brief 模拟断开 WiFi
 */
void mock_wifi_disconnect(void);

/**
 * @brief 获取当前 SSID
 * @return SSID 字符串，未连接时返回空字符串
 */
const char *mock_wifi_get_ssid(void);

#ifdef __cplusplus
}
#endif

/**
 * @file wifi_mgr.h
 * @brief WiFi 管理模块：STA/AP 模式切换、自动重连、事件发布
 */
#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 WiFi 管理器
 *
 * 初始化 WiFi 驱动和网络接口，但不启动连接。
 * 需要先调用此函数，然后才能使用其他接口。
 */
esp_err_t wifi_mgr_init(void);

/**
 * @brief 以 STA 模式连接 WiFi
 *
 * @param ssid WiFi 名称
 * @param password WiFi 密码
 * @return ESP_OK 成功启动连接流程（不代表已连接）
 */
esp_err_t wifi_mgr_connect_sta(const char *ssid, const char *password);

/**
 * @brief 启动 AP 模式（配网用）
 *
 * 创建热点：EReader-XXXX（XXXX 为 MAC 末四位）
 * 密码：ereader123
 * IP：192.168.4.1
 */
esp_err_t wifi_mgr_start_ap(void);

/**
 * @brief 停止 AP 模式
 */
esp_err_t wifi_mgr_stop_ap(void);

/**
 * @brief 断开当前连接
 */
esp_err_t wifi_mgr_disconnect(void);

/**
 * @brief 检查是否已连接
 */
bool wifi_mgr_is_connected(void);

/**
 * @brief 获取当前 IP 地址
 *
 * @param ip_str 输出缓冲区
 * @param max_len 缓冲区大小
 * @return ESP_OK 成功获取
 */
esp_err_t wifi_mgr_get_ip(char *ip_str, size_t max_len);

/**
 * @brief 扫描附近 WiFi
 *
 * @param results 输出扫描结果数组
 * @param count 输入：数组大小，输出：实际数量
 * @return ESP_OK 扫描成功
 */
esp_err_t wifi_mgr_scan(wifi_ap_record_t *results, uint16_t *count);

/**
 * @brief 获取当前连接的 SSID
 */
const char *wifi_mgr_get_ssid(void);

/**
 * @brief 获取当前连接的 RSSI（信号强度）
 */
int wifi_mgr_get_rssi(void);

#ifdef __cplusplus
}
#endif

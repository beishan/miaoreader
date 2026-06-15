/**
 * @file web_server.h
 * @brief 网页服务器模块：HTTP 路由和 API
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化网页服务器
 *
 * 配置 HTTP 服务器和路由。
 * 需要在 WiFi 连接后调用 web_server_start() 启动服务。
 */
esp_err_t web_server_init(void);

/**
 * @brief 启动网页服务器
 *
 * @return ESP_OK 启动成功
 */
esp_err_t web_server_start(void);

/**
 * @brief 停止网页服务器
 */
esp_err_t web_server_stop(void);

/**
 * @brief 检查服务器是否运行中
 */
bool web_server_is_running(void);

#ifdef __cplusplus
}
#endif

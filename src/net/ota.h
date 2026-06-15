/**
 * @file ota.h
 * @brief OTA 升级模块
 */
#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 OTA 模块
 */
esp_err_t ota_init(void);

/**
 * @brief OTA 升级请求处理函数
 *
 * 处理 POST /api/system/update 请求
 * 接收 multipart/form-data 格式的固件文件
 */
esp_err_t ota_update_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

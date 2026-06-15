/**
 * @file esp_err.h
 * @brief ESP 错误类型兼容层（PC 仿真用）
 */
#pragma once

#ifdef PC_SIMULATION

#include <stdint.h>

/* ESP 错误类型定义 */
typedef int esp_err_t;

#define ESP_OK          0
#define ESP_FAIL        -1
#define ESP_ERR_NO_MEM  0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_TIMEOUT 0x106

/* 错误转字符串（简化版） */
static inline const char *esp_err_to_name(esp_err_t err)
{
    if (err == ESP_OK) return "ESP_OK";
    if (err == ESP_FAIL) return "ESP_FAIL";
    return "ESP_ERR_UNKNOWN";
}

/* 错误检查宏 */
#define ESP_ERROR_CHECK(x) do { esp_err_t __err = (x); if (__err != ESP_OK) { } } while(0)

#endif /* PC_SIMULATION */

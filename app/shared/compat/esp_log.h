/**
 * @file esp_log.h
 * @brief ESP 日志兼容层（PC 仿真用）
 */
#pragma once

#ifdef PC_SIMULATION

#include <stdio.h>

/* 日志级别定义 */
#define ESP_LOG_ERROR   1
#define ESP_LOG_WARN    2
#define ESP_LOG_INFO    3
#define ESP_LOG_DEBUG   4
#define ESP_LOG_VERBOSE 5

/* 日志宏映射到 printf */
#define ESP_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) printf("[V][%s] " fmt "\n", tag, ##__VA_ARGS__)

#endif /* PC_SIMULATION */

/**
 * @file reading_progress.h
 * @brief 阅读进度存储抽象层
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化阅读进度存储
 */
esp_err_t reading_progress_init(void);

/**
 * @brief 保存阅读进度
 *
 * @param book_id 书籍 ID（文件路径 hash）
 * @param page 页码
 * @return ESP_OK 成功
 */
esp_err_t reading_progress_save(const char *book_id, uint32_t page);

/**
 * @brief 加载阅读进度
 *
 * @param book_id 书籍 ID
 * @param page 输出页码
 * @return ESP_OK 成功
 */
esp_err_t reading_progress_load(const char *book_id, uint32_t *page);

#ifdef __cplusplus
}
#endif

/**
 * @file book_meta.h
 * @brief 书籍元数据管理：SD:/books/metadata.json
 */
#pragma once

#include "engine/types.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOK_META_MAX 128

/**
 * @brief 初始化元数据模块
 *
 * 从 SD:/books/metadata.json 加载元数据。
 */
esp_err_t book_meta_init(void);

/**
 * @brief 保存元数据到 SD:/books/metadata.json
 */
esp_err_t book_meta_save(void);

/**
 * @brief 获取书籍数量
 */
int book_meta_get_count(void);

/**
 * @brief 获取指定索引的书籍元数据
 *
 * @param index 索引（0 到 count-1）
 * @param meta 输出元数据
 * @return ESP_OK 成功
 */
esp_err_t book_meta_get(int index, BookMeta *meta);

/**
 * @brief 根据文件路径查找书籍
 *
 * @param file_path 文件路径
 * @param meta 输出元数据
 * @return ESP_OK 找到
 */
esp_err_t book_meta_find_by_path(const char *file_path, BookMeta *meta);

/**
 * @brief 添加或更新书籍元数据
 *
 * 如果已存在则更新，否则添加。
 *
 * @param meta 元数据
 * @return ESP_OK 成功
 */
esp_err_t book_meta_upsert(const BookMeta *meta);

/**
 * @brief 删除书籍元数据
 *
 * @param file_path 文件路径
 * @return ESP_OK 成功
 */
esp_err_t book_meta_remove(const char *file_path);

/**
 * @brief 更新书籍的当前页码
 *
 * @param file_path 文件路径
 * @param current_page 当前页码
 * @return ESP_OK 成功
 */
esp_err_t book_meta_update_page(const char *file_path, uint32_t current_page);

/**
 * @brief 更新书籍的总页数
 *
 * @param file_path 文件路径
 * @param total_pages 总页数
 * @return ESP_OK 成功
 */
esp_err_t book_meta_update_total_pages(const char *file_path, uint32_t total_pages);

/**
 * @brief 增加书籍的阅读时间
 *
 * @param file_path 文件路径
 * @param seconds 增加的秒数
 * @return ESP_OK 成功
 */
esp_err_t book_meta_add_reading_time(const char *file_path, uint32_t seconds);

/**
 * @brief 扫描 SD:/books/ 目录，同步元数据
 *
 * 添加新发现的书籍，删除已不存在的书籍。
 */
esp_err_t book_meta_sync_from_sd(void);

/**
 * @brief 获取当前正在读的书（最近阅读的书）
 *
 * @param meta 输出元数据
 * @return ESP_OK 找到
 */
esp_err_t book_meta_get_current_reading(BookMeta *meta);

#ifdef __cplusplus
}
#endif

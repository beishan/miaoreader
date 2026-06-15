/**
 * @file bookmark.h
 * @brief 书签管理模块
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOKMARK_MAX 100
#define BOOKMARK_BOOK_NAME_LEN 64

typedef struct {
    char book_name[BOOKMARK_BOOK_NAME_LEN];
    uint32_t page_num;
    uint32_t timestamp;
} Bookmark;

/**
 * @brief 初始化书签模块
 */
esp_err_t bookmark_init(void);

/**
 * @brief 添加书签
 *
 * @param book_name 书名
 * @param page_num 页码
 * @return ESP_OK 添加成功
 */
esp_err_t bookmark_add(const char *book_name, uint32_t page_num);

/**
 * @brief 删除书签
 *
 * @param book_name 书名
 * @param page_num 页码
 * @return ESP_OK 删除成功
 */
esp_err_t bookmark_remove(const char *book_name, uint32_t page_num);

/**
 * @brief 获取指定书籍的所有书签
 *
 * @param book_name 书名
 * @param bookmarks 输出书签数组
 * @param max_count 数组最大容量
 * @param count 输出实际数量
 * @return ESP_OK 获取成功
 */
esp_err_t bookmark_get_list(const char *book_name, Bookmark *bookmarks,
                            int max_count, int *count);

/**
 * @brief 检查指定页码是否有书签
 *
 * @param book_name 书名
 * @param page_num 页码
 * @return true 有书签
 */
bool bookmark_exists(const char *book_name, uint32_t page_num);

#ifdef __cplusplus
}
#endif

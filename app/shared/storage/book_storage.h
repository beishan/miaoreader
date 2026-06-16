/**
 * @file book_storage.h
 * @brief JSON 持久化存储模块
 *
 * 管理阅读进度和书签数据，使用 JSON 格式存储。
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** 每本书最多书签数 */
#define BOOKMARK_MAX_PER_BOOK 20

/**
 * @brief 初始化存储模块
 *
 * 创建 data 目录，加载或创建 JSON 文件。
 */
void book_storage_init(void);

/**
 * @brief 保存阅读进度到内存
 *
 * @param book_name 书名
 * @param page 页码
 */
void book_storage_save_progress(const char *book_name, int page);

/**
 * @brief 加载阅读进度
 *
 * @param book_name 书名
 * @return 页码，-1 表示无记录
 */
int book_storage_load_progress(const char *book_name);

/**
 * @brief 添加书签
 *
 * 超出 BOOKMARK_MAX_PER_BOOK 个时删除最早的。
 *
 * @param book_name 书名
 * @param page 页码
 */
void book_storage_add_bookmark(const char *book_name, int page);

/**
 * @brief 获取书签列表
 *
 * @param book_name 书名
 * @param pages 输出页码数组
 * @param max_count 数组最大容量
 * @return 实际返回的书签数量
 */
int book_storage_get_bookmarks(const char *book_name, int *pages, int max_count);

/**
 * @brief 检查某页是否有书签
 *
 * @param book_name 书名
 * @param page 页码
 * @return 1 有书签，0 无书签
 */
int book_storage_has_bookmark(const char *book_name, int page);

/**
 * @brief 将内存数据写入 JSON 文件
 */
void book_storage_save(void);

#ifdef __cplusplus
}
#endif

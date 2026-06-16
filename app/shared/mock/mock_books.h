/**
 * @file mock_books.h
 * @brief Mock 书籍模块 - 模拟书库数据
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 书籍元数据结构
 */
typedef struct {
    char id[16];            /* 书籍 ID */
    char title[128];        /* 书名 */
    char author[64];        /* 作者 */
    char filePath[256];     /* 文件路径 */
    uint32_t totalPages;    /* 总页数 */
    uint32_t currentPage;   /* 当前页 */
    uint32_t readingSeconds;/* 阅读秒数 */
    bool isCurrentReading;  /* 是否正在阅读 */
} MockBookMeta;

/**
 * @brief 初始化 mock 书籍库
 */
void mock_books_init(void);

/**
 * @brief 获取书籍总数
 * @return 书籍数量
 */
int mock_books_get_count(void);

/**
 * @brief 获取指定索引的书籍
 * @param index 索引 (0-based)
 * @param book 输出书籍数据
 * @return 0 成功, -1 失败
 */
int mock_books_get_by_index(int index, MockBookMeta *book);

/**
 * @brief 获取当前正在阅读的书籍
 * @param book 输出书籍数据
 * @return 0 成功, -1 没有正在阅读的书
 */
int mock_books_get_current_reading(MockBookMeta *book);

/**
 * @brief 获取藏书数量字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_books_get_count_str(char *buf, int size);

/**
 * @brief 获取今日阅读时间字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_books_get_today_reading_str(char *buf, int size);

/**
 * @brief 获取累计阅读时间字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_books_get_total_reading_str(char *buf, int size);

/**
 * @brief 获取当前阅读书籍信息字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_books_get_current_book_str(char *buf, int size);

/**
 * @brief 获取阅读进度字符串
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 */
void mock_books_get_progress_str(char *buf, int size);

/**
 * @brief 获取阅读进度百分比
 * @return 进度百分比 (0-100)
 */
int mock_books_get_progress_percent(void);

/**
 * @brief 获取书籍文件路径
 * @param index 索引 (0-based)
 * @param path 输出路径缓冲区
 * @param size 缓冲区大小
 * @return 0 成功, -1 失败
 */
int mock_books_get_path(int index, char *path, int size);

/**
 * @brief 加载书籍文本（按需，调用者 free）
 * @param index 索引 (0-based)
 * @return malloc 分配的 UTF-8 字符串，调用者负责 free；失败返回 NULL
 */
char *mock_books_load_text(int index);

/**
 * @brief 获取书籍文件名（用于存储 key）
 * @param index 索引 (0-based)
 * @return 文件名字符串指针，失败返回 NULL
 */
const char *mock_books_get_filename(int index);

#ifdef __cplusplus
}
#endif

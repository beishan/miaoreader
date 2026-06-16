/**
 * @file book_parser.h
 * @brief 书籍解析器（TXT + EPUB）
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOOK_FMT_TXT,
    BOOK_FMT_EPUB,
    BOOK_FMT_UNKNOWN
} BookFormat;

typedef struct {
    char title[128];
    char author[64];
    int total_chars;
} BookInfo;

/**
 * @brief 检测书籍格式
 */
BookFormat book_parser_detect(const char *path);

/**
 * @brief 加载书籍文本（返回 malloc 的 UTF-8 字符串，调用者 free）
 */
char *book_parser_load_text(const char *path);

/**
 * @brief 提取书籍元数据
 */
int book_parser_extract_info(const char *path, BookInfo *info);

#ifdef __cplusplus
}
#endif

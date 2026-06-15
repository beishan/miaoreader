#pragma once
/**
 * @file book_parser.h
 * @brief 书籍文件解析：TXT（UTF-8/GBK 自动检测） + EPUB（ZIP+XML）
 *
 * 内存策略：解析后整本书以单一 UTF-8 buffer 返回（调用者 free）。
 * 不支持大文件流式分块（TXT 上限 8MB，EPUB 上限 32MB）。
 */
#include "engine/types.h"
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOOK_TXT,
    BOOK_EPUB,
    BOOK_UNKNOWN,
} BookFormat;

/* 检测文件格式（通过魔数 / 扩展名） */
BookFormat book_detect_format(const char *file_path);

/* 加载书籍文本（自动检测并转换编码） */
esp_err_t book_load_text(const char *file_path, char **text_out, uint32_t *len_out);

/* 提取元数据（书名/作者/路径） */
esp_err_t book_extract_metadata(const char *file_path, BookMeta *meta);

/* 释放 book_load_text 分配的文本 */
void book_free_text(char *text);

#ifdef __cplusplus
}
#endif

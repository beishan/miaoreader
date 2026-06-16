/**
 * @file book_parser.c
 * @brief 书籍解析器实现
 */

#ifdef PC_SIMULATION

#include "book_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FILE_SIZE (10 * 1024 * 1024)  /* 10MB */

/* UTF-8 BOM */
static const uint8_t UTF8_BOM[] = {0xEF, 0xBB, 0xBF};

/**
 * @brief 获取文件扩展名
 */
static const char *get_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return "";
    return dot + 1;
}

/**
 * @brief 检测是否为合法的 UTF-8
 *
 * 启发式检测：检查字节序列是否符合 UTF-8 编码规则
 * 返回 1 表示是 UTF-8，0 表示不是
 */
static int is_valid_utf8(const uint8_t *data, size_t len)
{
    size_t i = 0;
    size_t utf8_chars = 0;
    size_t invalid_chars = 0;

    while (i < len) {
        uint8_t c = data[i];

        if (c <= 0x7F) {
            /* ASCII */
            i++;
            utf8_chars++;
        } else if ((c & 0xE0) == 0xC0) {
            /* 2-byte sequence */
            if (i + 1 >= len || (data[i + 1] & 0xC0) != 0x80) {
                invalid_chars++;
                i++;
            } else {
                utf8_chars++;
                i += 2;
            }
        } else if ((c & 0xF0) == 0xE0) {
            /* 3-byte sequence */
            if (i + 2 >= len || (data[i + 1] & 0xC0) != 0x80 ||
                (data[i + 2] & 0xC0) != 0x80) {
                invalid_chars++;
                i++;
            } else {
                utf8_chars++;
                i += 3;
            }
        } else if ((c & 0xF8) == 0xF0) {
            /* 4-byte sequence */
            if (i + 3 >= len || (data[i + 1] & 0xC0) != 0x80 ||
                (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) {
                invalid_chars++;
                i++;
            } else {
                utf8_chars++;
                i += 4;
            }
        } else {
            invalid_chars++;
            i++;
        }
    }

    /* 如果无效字符超过 10%，认为不是 UTF-8 */
    if (utf8_chars + invalid_chars == 0) return 1;
    return (invalid_chars * 100 / (utf8_chars + invalid_chars)) < 10;
}

/**
 * @brief 计算 GBK 转 UTF-8 后的输出大小
 */
static size_t gbk_to_utf8_size(const uint8_t *data, size_t len)
{
    size_t out_size = 0;
    size_t i = 0;

    while (i < len) {
        uint8_t c = data[i];
        if (c <= 0x7F) {
            /* ASCII */
            out_size++;
            i++;
        } else if (c >= 0x81 && c <= 0xFE && i + 1 < len &&
                   data[i + 1] >= 0x40 && data[i + 1] <= 0xFE &&
                   data[i + 1] != 0x7F) {
            /* GBK double-byte */
            out_size += 3;  /* CJK chars -> 3 bytes in UTF-8 */
            i += 2;
        } else {
            /* 单字节，直接复制 */
            out_size++;
            i++;
        }
    }
    return out_size;
}

/**
 * @brief GBK 转 UTF-8
 *
 * 对于 CJK 汉字区域，使用简化映射：
 * (c1-0x81)*191 + (c2-0x40) + 0x4E00
 */
static int gbk_to_utf8(const uint8_t *data, size_t in_len,
                        char *out, size_t out_size)
{
    size_t oi = 0;
    size_t i = 0;

    while (i < in_len && oi < out_size - 1) {
        uint8_t c = data[i];

        if (c <= 0x7F) {
            /* ASCII */
            out[oi++] = (char)c;
            i++;
        } else if (c >= 0x81 && c <= 0xFE && i + 1 < in_len &&
                   data[i + 1] >= 0x40 && data[i + 1] <= 0xFE &&
                   data[i + 1] != 0x7F) {
            /* GBK double-byte -> CJK Unicode */
            uint32_t unicode = (uint32_t)(c - 0x81) * 191 +
                               (uint32_t)(data[i + 1] - 0x40) + 0x4E00;

            if (oi + 3 >= out_size) break;

            /* UTF-8 encoding */
            out[oi++] = (char)(0xE0 | (unicode >> 12));
            out[oi++] = (char)(0x80 | ((unicode >> 6) & 0x3F));
            out[oi++] = (char)(0x80 | (unicode & 0x3F));
            i += 2;
        } else {
            /* 未知字节，直接复制 */
            out[oi++] = (char)c;
            i++;
        }
    }

    out[oi] = '\0';
    return (int)oi;
}

/**
 * @brief 处理换行符：\r\n -> \n
 */
static char *normalize_line_endings(const char *text, size_t len)
{
    char *out = malloc(len + 1);
    if (!out) return NULL;

    size_t oi = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\r') {
            if (i + 1 < len && text[i + 1] == '\n') {
                continue;  /* 跳过 \r，保留后面的 \n */
            }
            out[oi++] = '\n';  /* 单独的 \r 转为 \n */
        } else {
            out[oi++] = text[i];
        }
    }
    out[oi] = '\0';
    return out;
}

BookFormat book_parser_detect(const char *path)
{
    const char *ext = get_extension(path);

    if (strcasecmp(ext, "txt") == 0) {
        return BOOK_FMT_TXT;
    } else if (strcasecmp(ext, "epub") == 0) {
        return BOOK_FMT_EPUB;
    }

    return BOOK_FMT_UNKNOWN;
}

char *book_parser_load_text(const char *path)
{
    FILE *fp;
    long file_size;
    uint8_t *raw_data;
    char *result = NULL;
    size_t bytes_read;

    /* 只支持 TXT 格式 */
    if (book_parser_detect(path) != BOOK_FMT_TXT) {
        return NULL;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* 检查文件大小限制 */
    if (file_size <= 0 || file_size > MAX_FILE_SIZE) {
        fclose(fp);
        return NULL;
    }

    /* 读取文件 */
    raw_data = malloc((size_t)file_size + 1);
    if (!raw_data) {
        fclose(fp);
        return NULL;
    }

    bytes_read = fread(raw_data, 1, (size_t)file_size, fp);
    fclose(fp);

    if (bytes_read == 0) {
        free(raw_data);
        return NULL;
    }

    raw_data[bytes_read] = '\0';

    /* 跳过 UTF-8 BOM */
    size_t offset = 0;
    if (bytes_read >= 3 &&
        raw_data[0] == UTF8_BOM[0] &&
        raw_data[1] == UTF8_BOM[1] &&
        raw_data[2] == UTF8_BOM[2]) {
        offset = 3;
    }

    /* 启发式检测编码 */
    if (is_valid_utf8(raw_data + offset, bytes_read - offset)) {
        /* 是 UTF-8，直接处理换行符 */
        result = normalize_line_endings((char *)raw_data + offset,
                                        bytes_read - offset);
    } else {
        /* 可能是 GBK，转换为 UTF-8 */
        size_t utf8_size = gbk_to_utf8_size(raw_data + offset,
                                              bytes_read - offset);
        char *utf8_buf = malloc(utf8_size + 1);
        if (utf8_buf) {
            gbk_to_utf8(raw_data + offset, bytes_read - offset,
                        utf8_buf, utf8_size + 1);
            result = normalize_line_endings(utf8_buf, strlen(utf8_buf));
            free(utf8_buf);
        }
    }

    free(raw_data);
    return result;
}

int book_parser_extract_info(const char *path, BookInfo *info)
{
    const char *filename;
    const char *dot;
    size_t name_len;

    if (!path || !info) return -1;

    memset(info, 0, sizeof(BookInfo));

    /* 从文件路径中提取文件名 */
    filename = strrchr(path, '/');
    if (!filename) {
        filename = strrchr(path, '\\');
    }
    if (filename) {
        filename++;  /* 跳过路径分隔符 */
    } else {
        filename = path;
    }

    /* 去掉扩展名作为标题 */
    dot = strrchr(filename, '.');
    if (dot && dot != filename) {
        name_len = (size_t)(dot - filename);
    } else {
        name_len = strlen(filename);
    }

    if (name_len >= sizeof(info->title)) {
        name_len = sizeof(info->title) - 1;
    }
    memcpy(info->title, filename, name_len);
    info->title[name_len] = '\0';

    /* 作者未知 */
    strncpy(info->author, "未知", sizeof(info->author) - 1);

    /* 统计字符数 */
    char *text = book_parser_load_text(path);
    if (text) {
        info->total_chars = (int)strlen(text);
        free(text);
    }

    return 0;
}

#endif /* PC_SIMULATION */

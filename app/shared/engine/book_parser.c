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
#define MAX_EPUB_SIZE (50 * 1024 * 1024)  /* 50MB for EPUB */

/* UTF-8 BOM */
static const uint8_t UTF8_BOM[] = {0xEF, 0xBB, 0xBF};

/* ========== ZIP 结构定义 ========== */

#pragma pack(push, 1)
typedef struct {
    uint32_t signature;       /* 0x04034b50 */
    uint16_t version;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
} ZipLocalHeader;

typedef struct {
    uint32_t signature;       /* 0x02014b50 */
    uint16_t version_made;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
    uint16_t comment_len;
    uint16_t disk_start;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t local_offset;
} ZipCentralDirEntry;

typedef struct {
    uint32_t signature;       /* 0x06054b50 */
    uint16_t disk_num;
    uint16_t cd_disk;
    uint16_t cd_entries_disk;
    uint16_t cd_entries_total;
    uint32_t cd_size;
    uint32_t cd_offset;
    uint16_t comment_len;
} ZipEOCD;
#pragma pack(pop)

#define ZIP_SIG_LOCAL     0x04034b50
#define ZIP_SIG_CENTRAL   0x02014b50
#define ZIP_SIG_EOCD      0x06054b50
#define ZIP_COMP_STORED   0

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

/* ========== ZIP 读取函数 ========== */

/**
 * @brief 从文件末尾搜索 EOCD 签名，找到中央目录位置
 */
static int zip_find_central_dir(FILE *f, long *cd_offset, int *cd_count)
{
    long file_size;
    long search_offset;
    uint8_t buf[65536 + 22];  /* EOCD 最大搜索范围: comment(64KB) + EOCD(22) */
    size_t search_len;

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);

    /* EOCD 至少 22 字节，comment 最多 65535 字节 */
    search_len = (file_size < (long)sizeof(buf)) ? (size_t)file_size : sizeof(buf);
    search_offset = file_size - (long)search_len;

    fseek(f, search_offset, SEEK_SET);
    if (fread(buf, 1, search_len, f) != search_len) {
        return -1;
    }

    /* 从后往前搜索 EOCD 签名 */
    for (size_t i = search_len - 22; i > 0; i--) {
        if (buf[i] == 0x50 && buf[i + 1] == 0x4B &&
            buf[i + 2] == 0x05 && buf[i + 3] == 0x06) {
            ZipEOCD eocd;
            memcpy(&eocd, buf + i, sizeof(ZipEOCD));
            *cd_offset = (long)eocd.cd_offset;
            *cd_count = (int)eocd.cd_entries_total;
            return 0;
        }
    }

    return -1;
}

/**
 * @brief 从 ZIP 中读取 stored（未压缩）条目数据
 */
static uint8_t *zip_read_stored(FILE *f, ZipCentralDirEntry *entry, size_t *out_size)
{
    ZipLocalHeader local_hdr;
    uint8_t *data;

    if (entry->compression != ZIP_COMP_STORED) {
        return NULL;  /* 只支持 stored 模式 */
    }

    fseek(f, (long)entry->local_offset, SEEK_SET);
    if (fread(&local_hdr, sizeof(local_hdr), 1, f) != 1) {
        return NULL;
    }

    if (local_hdr.signature != ZIP_SIG_LOCAL) {
        return NULL;
    }

    /* 跳过文件名和扩展字段 */
    fseek(f, (long)local_hdr.filename_len + (long)local_hdr.extra_len, SEEK_CUR);

    data = malloc(entry->uncompressed_size + 1);
    if (!data) {
        return NULL;
    }

    if (fread(data, 1, entry->uncompressed_size, f) != entry->uncompressed_size) {
        free(data);
        return NULL;
    }

    data[entry->uncompressed_size] = '\0';
    *out_size = entry->uncompressed_size;
    return data;
}

/* ========== HTML 标签剥离 ========== */

/**
 * @brief 去掉 HTML 标签，提取纯文本
 *
 * 处理 <br> <p> -> 换行
 * 处理 HTML 实体：&amp; &lt; &gt; &nbsp; &quot;
 */
static char *strip_html(const char *html, size_t len)
{
    char *out;
    size_t oi;
    int in_tag;
    int in_entity;
    char entity[16];
    int entity_len;

    out = malloc(len + 1);
    if (!out) return NULL;

    oi = 0;
    in_tag = 0;
    in_entity = 0;
    entity_len = 0;

    for (size_t i = 0; i < len; i++) {
        char c = html[i];

        if (in_tag) {
            if (c == '>') {
                in_tag = 0;
            }
            continue;
        }

        if (in_entity) {
            if (c == ';') {
                entity[entity_len] = '\0';
                if (strcmp(entity, "amp") == 0) {
                    out[oi++] = '&';
                } else if (strcmp(entity, "lt") == 0) {
                    out[oi++] = '<';
                } else if (strcmp(entity, "gt") == 0) {
                    out[oi++] = '>';
                } else if (strcmp(entity, "nbsp") == 0) {
                    out[oi++] = ' ';
                } else if (strcmp(entity, "quot") == 0) {
                    out[oi++] = '"';
                } else {
                    /* 未知实体，原样输出 */
                    out[oi++] = '&';
                    memcpy(out + oi, entity, (size_t)entity_len);
                    oi += (size_t)entity_len;
                    out[oi++] = ';';
                }
                in_entity = 0;
                entity_len = 0;
            } else if (entity_len < (int)sizeof(entity) - 1) {
                entity[entity_len++] = c;
            }
            continue;
        }

        if (c == '<') {
            /* 检查是否为 <br>, <br/>, <p>, </p>, <div>, </div> 等块级标签 */
            if (strncasecmp(html + i, "<br", 3) == 0 ||
                strncasecmp(html + i, "<p>", 3) == 0 ||
                strncasecmp(html + i, "<p ", 3) == 0 ||
                strncasecmp(html + i, "</p>", 4) == 0 ||
                strncasecmp(html + i, "<div", 4) == 0 ||
                strncasecmp(html + i, "</div>", 6) == 0) {
                out[oi++] = '\n';
            }
            in_tag = 1;
            continue;
        }

        if (c == '&') {
            in_entity = 1;
            entity_len = 0;
            continue;
        }

        out[oi++] = c;
    }

    out[oi] = '\0';
    return out;
}

/* ========== EPUB 解析内部函数 ========== */

/**
 * @brief 在 ZIP 中央目录中查找指定文件名的条目
 */
static int zip_find_entry(FILE *f, long cd_offset, int cd_count,
                          const char *filename, ZipCentralDirEntry *out_entry)
{
    fseek(f, cd_offset, SEEK_SET);

    for (int i = 0; i < cd_count; i++) {
        ZipCentralDirEntry entry;
        char name_buf[512];
        size_t name_len;

        if (fread(&entry, sizeof(entry), 1, f) != 1) {
            return -1;
        }

        if (entry.signature != ZIP_SIG_CENTRAL) {
            return -1;
        }

        name_len = entry.filename_len;
        if (name_len >= sizeof(name_buf)) {
            name_len = sizeof(name_buf) - 1;
        }

        if (fread(name_buf, 1, name_len, f) != name_len) {
            return -1;
        }
        name_buf[name_len] = '\0';

        if (strcmp(name_buf, filename) == 0) {
            *out_entry = entry;
            return 0;
        }

        /* 跳过 extra 和 comment */
        fseek(f, (long)entry.extra_len + (long)entry.comment_len, SEEK_CUR);
    }

    return -1;
}

/**
 * @brief 从 XML 字符串中提取指定标签的内容
 *
 * 查找 <tag_name>...</tag_name> 或 <tag_name attr>...</tag_name>
 * 返回 malloc 的字符串，调用者 free
 */
static char *xml_get_tag_content(const char *xml, const char *tag_name)
{
    char open_tag[64];
    char close_tag[64];
    const char *start, *end;
    size_t tag_len;

    snprintf(open_tag, sizeof(open_tag), "<%s", tag_name);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag_name);

    /* 查找开始标签 */
    start = strstr(xml, open_tag);
    if (!start) return NULL;

    /* 找到 '>' 结束 */
    start = strchr(start, '>');
    if (!start) return NULL;
    start++;  /* 跳过 '>' */

    /* 查找结束标签 */
    end = strstr(start, close_tag);
    if (!end) return NULL;

    tag_len = (size_t)(end - start);
    char *result = malloc(tag_len + 1);
    if (!result) return NULL;

    memcpy(result, start, tag_len);
    result[tag_len] = '\0';
    return result;
}

/**
 * @brief 从 container.xml 中提取 OPF 文件路径（rootfile full-path）
 */
static char *epub_get_opf_path(const char *container_xml)
{
    const char *pos;
    const char *start;
    const char *end;
    size_t len;
    char *result;

    /* 查找 full-path="..." */
    pos = strstr(container_xml, "full-path");
    if (!pos) return NULL;

    start = strchr(pos, '"');
    if (!start) return NULL;
    start++;

    end = strchr(start, '"');
    if (!end) return NULL;

    len = (size_t)(end - start);
    result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/**
 * @brief 简单的 manifest 条目结构
 */
typedef struct {
    char id[64];
    char href[256];
} ManifestItem;

#define MAX_MANIFEST_ITEMS 256

/**
 * @brief 从 OPF 中提取 manifest 项（id -> href 映射）
 *
 * 返回 malloc 的 ManifestItem 数组，*count 返回数量
 */
static ManifestItem *epub_parse_manifest(const char *opf, int *count)
{
    ManifestItem *items;
    const char *pos;
    int n = 0;

    items = malloc(sizeof(ManifestItem) * MAX_MANIFEST_ITEMS);
    if (!items) return NULL;

    pos = opf;
    while (n < MAX_MANIFEST_ITEMS) {
        const char *item_start;
        const char *id_attr, *href_attr;

        item_start = strstr(pos, "<item ");
        if (!item_start) break;

        /* 提取 id */
        id_attr = strstr(item_start, "id=\"");
        if (!id_attr || id_attr > strstr(item_start, ">")) {
            pos = item_start + 1;
            continue;
        }
        id_attr += 4;
        const char *id_end = strchr(id_attr, '"');
        if (!id_end) { pos = item_start + 1; continue; }

        /* 提取 href */
        href_attr = strstr(item_start, "href=\"");
        if (!href_attr || href_attr > strstr(item_start, ">")) {
            pos = item_start + 1;
            continue;
        }
        href_attr += 6;
        const char *href_end = strchr(href_attr, '"');
        if (!href_end) { pos = item_start + 1; continue; }

        size_t id_len = (size_t)(id_end - id_attr);
        size_t href_len = (size_t)(href_end - href_attr);

        if (id_len < sizeof(items[n].id) && href_len < sizeof(items[n].href)) {
            memcpy(items[n].id, id_attr, id_len);
            items[n].id[id_len] = '\0';
            memcpy(items[n].href, href_attr, href_len);
            items[n].href[href_len] = '\0';
            n++;
        }

        pos = href_end + 1;
    }

    *count = n;
    return items;
}

/**
 * @brief 根据 id 在 manifest 数组中查找 href
 */
static const char *manifest_find_href(ManifestItem *items, int count, const char *id)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].id, id) == 0) {
            return items[i].href;
        }
    }
    return NULL;
}

/**
 * @brief 从 OPF 中提取 spine 的 idref 列表（阅读顺序）
 *
 * 返回 malloc 的字符串数组（每个元素是 idref），*count 返回数量
 */
static char **epub_parse_spine(const char *opf, int *count)
{
    char **idrefs;
    const char *pos;
    int n = 0;

    idrefs = malloc(sizeof(char *) * MAX_MANIFEST_ITEMS);
    if (!idrefs) return NULL;

    pos = opf;
    while (n < MAX_MANIFEST_ITEMS) {
        const char *ref_start;
        const char *idref_attr;

        ref_start = strstr(pos, "<itemref");
        if (!ref_start) break;

        idref_attr = strstr(ref_start, "idref=\"");
        if (!idref_attr || idref_attr > strchr(ref_start, '>')) {
            pos = ref_start + 1;
            continue;
        }
        idref_attr += 7;
        const char *idref_end = strchr(idref_attr, '"');
        if (!idref_end) { pos = ref_start + 1; continue; }

        size_t len = (size_t)(idref_end - idref_attr);
        idrefs[n] = malloc(len + 1);
        if (!idrefs[n]) { pos = idref_end + 1; continue; }

        memcpy(idrefs[n], idref_attr, len);
        idrefs[n][len] = '\0';
        n++;

        pos = idref_end + 1;
    }

    *count = n;
    return idrefs;
}

/**
 * @brief 获取 OPF 文件所在目录的前缀（用于拼接 href）
 *
 * 例如 "OEBPS/content.opf" -> "OEBPS/"
 * 根目录的 OPF 返回空字符串
 */
static void epub_get_opf_dir(const char *opf_path, char *dir, size_t dir_size)
{
    const char *last_slash = strrchr(opf_path, '/');
    if (last_slash) {
        size_t len = (size_t)(last_slash - opf_path + 1);
        if (len >= dir_size) len = dir_size - 1;
        memcpy(dir, opf_path, len);
        dir[len] = '\0';
    } else {
        dir[0] = '\0';
    }
}

/**
 * @brief EPUB 解析主流程
 */
static char *epub_load_text(const char *path)
{
    FILE *f;
    long cd_offset;
    int cd_count;
    ZipCentralDirEntry entry;
    uint8_t *container_data;
    size_t container_size;
    char *opf_path;
    uint8_t *opf_data;
    size_t opf_size;
    ManifestItem *manifest;
    int manifest_count;
    char **spine_idrefs;
    int spine_count;
    char opf_dir[256];
    char *result;
    size_t result_cap;
    size_t result_len;

    f = fopen(path, "rb");
    if (!f) return NULL;

    /* 1. 查找中央目录 */
    if (zip_find_central_dir(f, &cd_offset, &cd_count) != 0) {
        fclose(f);
        return NULL;
    }

    /* 2. 读取 META-INF/container.xml */
    if (zip_find_entry(f, cd_offset, cd_count, "META-INF/container.xml", &entry) != 0) {
        fclose(f);
        return NULL;
    }

    container_data = zip_read_stored(f, &entry, &container_size);
    if (!container_data) {
        fclose(f);
        return NULL;
    }

    /* 3. 提取 OPF 路径 */
    opf_path = epub_get_opf_path((char *)container_data);
    free(container_data);
    if (!opf_path) {
        fclose(f);
        return NULL;
    }

    /* 4. 读取 OPF 文件 */
    if (zip_find_entry(f, cd_offset, cd_count, opf_path, &entry) != 0) {
        free(opf_path);
        fclose(f);
        return NULL;
    }

    opf_data = zip_read_stored(f, &entry, &opf_size);
    if (!opf_data) {
        free(opf_path);
        fclose(f);
        return NULL;
    }

    /* 5. 解析 manifest 和 spine */
    epub_get_opf_dir(opf_path, opf_dir, sizeof(opf_dir));
    free(opf_path);

    manifest = epub_parse_manifest((char *)opf_data, &manifest_count);
    spine_idrefs = epub_parse_spine((char *)opf_data, &spine_count);
    free(opf_data);

    if (!manifest || !spine_idrefs || spine_count == 0) {
        free(manifest);
        if (spine_idrefs) {
            for (int i = 0; i < spine_count; i++) free(spine_idrefs[i]);
            free(spine_idrefs);
        }
        fclose(f);
        return NULL;
    }

    /* 6. 按 spine 顺序提取并拼接文本 */
    result_cap = 65536;
    result = malloc(result_cap);
    if (!result) {
        free(manifest);
        for (int i = 0; i < spine_count; i++) free(spine_idrefs[i]);
        free(spine_idrefs);
        fclose(f);
        return NULL;
    }
    result[0] = '\0';
    result_len = 0;

    for (int i = 0; i < spine_count; i++) {
        const char *href;
        char full_path[512];
        uint8_t *xhtml_data;
        size_t xhtml_size;
        char *text;

        href = manifest_find_href(manifest, manifest_count, spine_idrefs[i]);
        if (!href) continue;

        /* 拼接完整路径 */
        snprintf(full_path, sizeof(full_path), "%s%s", opf_dir, href);

        if (zip_find_entry(f, cd_offset, cd_count, full_path, &entry) != 0) {
            continue;
        }

        xhtml_data = zip_read_stored(f, &entry, &xhtml_size);
        if (!xhtml_data) continue;

        /* 剥离 HTML 标签 */
        text = strip_html((char *)xhtml_data, xhtml_size);
        free(xhtml_data);

        if (!text) continue;

        /* 拼接到结果 */
        size_t text_len = strlen(text);
        if (result_len + text_len + 2 >= result_cap) {
            result_cap = (result_len + text_len + 2) * 2;
            char *new_result = realloc(result, result_cap);
            if (!new_result) {
                free(text);
                break;
            }
            result = new_result;
        }

        memcpy(result + result_len, text, text_len);
        result_len += text_len;
        /* 章节之间加换行 */
        result[result_len++] = '\n';
        result[result_len] = '\0';

        free(text);
    }

    fclose(f);

    /* 清理 */
    free(manifest);
    for (int i = 0; i < spine_count; i++) free(spine_idrefs[i]);
    free(spine_idrefs);

    if (result_len == 0) {
        free(result);
        return NULL;
    }

    return result;
}

/**
 * @brief 从 EPUB 的 OPF 中提取书名和作者
 */
static int epub_extract_info(const char *path, BookInfo *info)
{
    FILE *f;
    long cd_offset;
    int cd_count;
    ZipCentralDirEntry entry;
    uint8_t *container_data;
    size_t container_size;
    char *opf_path;
    uint8_t *opf_data;
    size_t opf_size;
    char *val;

    f = fopen(path, "rb");
    if (!f) return -1;

    if (zip_find_central_dir(f, &cd_offset, &cd_count) != 0) {
        fclose(f);
        return -1;
    }

    /* 读取 container.xml */
    if (zip_find_entry(f, cd_offset, cd_count, "META-INF/container.xml", &entry) != 0) {
        fclose(f);
        return -1;
    }

    container_data = zip_read_stored(f, &entry, &container_size);
    if (!container_data) {
        fclose(f);
        return -1;
    }

    opf_path = epub_get_opf_path((char *)container_data);
    free(container_data);
    if (!opf_path) {
        fclose(f);
        return -1;
    }

    /* 读取 OPF */
    if (zip_find_entry(f, cd_offset, cd_count, opf_path, &entry) != 0) {
        free(opf_path);
        fclose(f);
        return -1;
    }

    opf_data = zip_read_stored(f, &entry, &opf_size);
    free(opf_path);
    fclose(f);

    if (!opf_data) return -1;

    /* 提取 dc:title */
    val = xml_get_tag_content((char *)opf_data, "dc:title");
    if (val) {
        strncpy(info->title, val, sizeof(info->title) - 1);
        info->title[sizeof(info->title) - 1] = '\0';
        free(val);
    }

    /* 提取 dc:creator */
    val = xml_get_tag_content((char *)opf_data, "dc:creator");
    if (val) {
        strncpy(info->author, val, sizeof(info->author) - 1);
        info->author[sizeof(info->author) - 1] = '\0';
        free(val);
    }

    free(opf_data);
    return 0;
}

BookFormat book_parser_detect(const char *path)
{
    if (!path) return BOOK_FMT_UNKNOWN;

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
    BookFormat fmt;

    if (!path) return NULL;

    fmt = book_parser_detect(path);

    /* EPUB 格式走专用解析 */
    if (fmt == BOOK_FMT_EPUB) {
        return epub_load_text(path);
    }

    /* 只支持 TXT 格式 */
    if (fmt != BOOK_FMT_TXT) {
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
    BookFormat fmt;

    if (!path || !info) return -1;

    memset(info, 0, sizeof(BookInfo));

    fmt = book_parser_detect(path);

    /* EPUB 从 OPF 提取元数据 */
    if (fmt == BOOK_FMT_EPUB) {
        if (epub_extract_info(path, info) == 0) {
            /* 统计字符数 */
            char *text = epub_load_text(path);
            if (text) {
                info->total_chars = (int)strlen(text);
                free(text);
            }
            return 0;
        }
        /* 如果解析失败，回退到文件名 */
    }

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

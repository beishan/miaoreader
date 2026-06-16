# 核心功能实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 实现 6 个核心功能（书籍加载、进度持久化、设置交互、上下文菜单、书签、EPUB），让 PC 仿真阅读器体验完整。

**架构：** 新增 engine 层（book_parser、bookmark_mgr）和 storage 层（book_storage），改造 mock_books 从本地目录扫描书籍，增强 page_reader 和 page_settings 的交互逻辑。

**技术栈：** C 语言、SDL2 仿真、JSON 文件存储、FreeType 渲染

---

## 文件结构

### 新增文件

| 文件 | 职责 |
|------|------|
| `shared/engine/book_parser.h` | 书籍解析接口（格式检测、文本加载、元数据提取） |
| `shared/engine/book_parser.c` | TXT + EPUB 解析实现 |
| `shared/storage/book_storage.h` | 统一 JSON 存储接口（进度 + 书签） |
| `shared/storage/book_storage.c` | JSON 读写实现 |
| `shared/engine/bookmark_mgr.h` | 书签管理接口 |
| `shared/engine/bookmark_mgr.c` | 书签管理实现 |
| `shared/books/` | 书籍文件目录（.txt / .epub） |
| `shared/data/` | 运行时数据目录（reading_data.json） |

### 修改文件

| 文件 | 改动 |
|------|------|
| `shared/mock/mock_books.c` | 从本地目录扫描书籍，使用 book_parser |
| `shared/ui/page_settings.c` | 添加 BROWSE/EDIT 状态机 + 交互逻辑 |
| `shared/ui/page_reader.c` | 添加上下文菜单 + 书签 + 进度持久化 |
| `shared/ui/page_bookshelf.c` | 使用真实书籍列表 |
| `sim_main.c` | 集成 book_storage 初始化 |
| `Makefile` | 添加新源文件 |

---

## 任务 1：book_parser — TXT 解析

**文件：**
- 创建：`shared/engine/book_parser.h`
- 创建：`shared/engine/book_parser.c`

### 步骤 1：创建 book_parser.h 接口定义

```c
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
```

### 步骤 2：创建 book_parser.c 基础框架 + TXT 格式检测

```c
/**
 * @file book_parser.c
 * @brief 书籍解析器实现
 */
#ifdef PC_SIMULATION

#include "book_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BookFormat book_parser_detect(const char *path)
{
    if (!path) return BOOK_FMT_UNKNOWN;

    const char *ext = strrchr(path, '.');
    if (!ext) return BOOK_FMT_UNKNOWN;

    if (strcasecmp(ext, ".txt") == 0) return BOOK_FMT_TXT;
    if (strcasecmp(ext, ".epub") == 0) return BOOK_FMT_EPUB;

    return BOOK_FMT_UNKNOWN;
}

/* GBK 高位字节检测 */
static int is_gbk_lead(unsigned char c)
{
    return c >= 0x81 && c <= 0xFE;
}

/* 简化 GBK → UTF-8 转换（区间映射） */
static int gbk_to_utf8(unsigned char c1, unsigned char c2, char *out)
{
    /* 简化映射：直接用 Unicode CJK 区域 */
    uint32_t codepoint = (uint32_t)(c1 - 0x81) * 191 + (c2 - 0x40) + 0x4E00;
    if (codepoint > 0x9FFF) codepoint = 0x4E00; /* 回退到常用字 */

    if (codepoint < 0x800) {
        out[0] = 0xC0 | (codepoint >> 6);
        out[1] = 0x80 | (codepoint & 0x3F);
        return 2;
    } else {
        out[0] = 0xE0 | (codepoint >> 12);
        out[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        out[2] = 0x80 | (codepoint & 0x3F);
        return 3;
    }
}

/* 检测是否为 UTF-8 */
static int is_utf8(const unsigned char *data, size_t len)
{
    /* 检查 BOM */
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return 1;
    }
    /* 启发式：检查是否有合法 UTF-8 序列 */
    int utf8_count = 0;
    for (size_t i = 0; i < len && i < 1024; i++) {
        if (data[i] >= 0xC0 && data[i] <= 0xDF) {
            if (i + 1 < len && (data[i+1] & 0xC0) == 0x80) utf8_count++;
        } else if (data[i] >= 0xE0 && data[i] <= 0xEF) {
            if (i + 2 < len && (data[i+1] & 0xC0) == 0x80 && (data[i+2] & 0xC0) == 0x80) utf8_count++;
        }
    }
    return utf8_count > 2;
}

char *book_parser_load_text(const char *path)
{
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 10 * 1024 * 1024) { /* 最大 10MB */
        fclose(f);
        return NULL;
    }

    unsigned char *raw = (unsigned char *)malloc(file_size + 1);
    if (!raw) { fclose(f); return NULL; }
    fread(raw, 1, file_size, f);
    raw[file_size] = '\0';
    fclose(f);

    /* 跳过 UTF-8 BOM */
    size_t start = 0;
    if (file_size >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
        start = 3;
    }

    /* 检测编码 */
    int utf8 = is_utf8(raw + start, file_size - start);

    if (utf8) {
        /* 已经是 UTF-8，直接返回 */
        char *result = (char *)malloc(file_size - start + 1);
        if (!result) { free(raw); return NULL; }
        memcpy(result, raw + start, file_size - start);
        result[file_size - start] = '\0';
        free(raw);
        return result;
    }

    /* GBK → UTF-8 转换 */
    /* 最坏情况：每个 GBK 字符变 3 字节 UTF-8 */
    char *out = (char *)malloc((file_size - start) * 3 + 1);
    if (!out) { free(raw); return NULL; }

    size_t oi = 0;
    for (size_t i = start; i < (size_t)file_size; ) {
        if (is_gbk_lead(raw[i]) && i + 1 < (size_t)file_size) {
            oi += gbk_to_utf8(raw[i], raw[i+1], out + oi);
            i += 2;
        } else if (raw[i] == '\r' && i + 1 < (size_t)file_size && raw[i+1] == '\n') {
            out[oi++] = '\n';
            i += 2;
        } else {
            out[oi++] = raw[i];
            i++;
        }
    }
    out[oi] = '\0';
    free(raw);

    /* 缩小分配 */
    char *result = (char *)realloc(out, oi + 1);
    return result ? result : out;
}

int book_parser_extract_info(const char *path, BookInfo *info)
{
    if (!path || !info) return -1;
    memset(info, 0, sizeof(BookInfo));

    /* 从文件名提取标题 */
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    strncpy(info->title, name, sizeof(info->title) - 1);

    /* 去掉扩展名 */
    char *dot = strrchr(info->title, '.');
    if (dot) *dot = '\0';

    /* 加载文本统计字符数 */
    char *text = book_parser_load_text(path);
    if (text) {
        info->total_chars = strlen(text);
        free(text);
    }

    strncpy(info->author, "未知", sizeof(info->author) - 1);
    return 0;
}

#endif /* PC_SIMULATION */
```

### 步骤 3：更新 Makefile 添加 book_parser.c

在 `SOURCES` 列表中添加：
```
          shared/engine/book_parser.c \
```

### 步骤 4：创建测试书籍目录和文件

创建 `shared/books/` 目录，放入一个测试 TXT 文件。

### 步骤 5：编译验证

运行：`make clean && make 2>&1`
预期：编译通过，无错误。

### 步骤 6：Commit

```bash
git add -A
git commit -m "feat: 添加 book_parser TXT 解析模块

- 格式检测（.txt/.epub）
- UTF-8 BOM 跳过
- GBK 启发式检测和转换
- 文本加载和元数据提取"
```

---

## 任务 2：book_parser — EPUB 解析

**文件：**
- 修改：`shared/engine/book_parser.c`

### 步骤 1：添加 ZIP 中央目录解析

在 `book_parser.c` 中添加：

```c
/* ZIP 结构定义 */
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
```

### 步骤 2：添加 ZIP 文件读取函数

```c
/* 从 ZIP 中读取指定条目的数据（仅 stored 模式） */
static void *zip_read_stored(FILE *f, ZipCentralDirEntry *entry, size_t *out_size)
{
    if (entry->compression != 0) {
        /* 不支持压缩 */
        *out_size = 0;
        return NULL;
    }

    fseek(f, entry->local_offset, SEEK_SET);
    ZipLocalHeader local;
    if (fread(&local, sizeof(local), 1, f) != 1) {
        *out_size = 0;
        return NULL;
    }

    fseek(f, entry->local_offset + sizeof(local) + local.filename_len + local.extra_len, SEEK_SET);

    void *data = malloc(entry->uncompressed_size + 1);
    if (!data) { *out_size = 0; return NULL; }

    fread(data, 1, entry->uncompressed_size, f);
    ((char *)data)[entry->uncompressed_size] = '\0';
    *out_size = entry->uncompressed_size;
    return data;
}

/* 查找 ZIP 中央目录 */
static int zip_find_central_dir(FILE *f, long *cd_offset, int *cd_count)
{
    /* 从文件末尾搜索 EOCD 签名 */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);

    long search_start = file_size - 65536 - sizeof(ZipEOCD);
    if (search_start < 0) search_start = 0;

    for (long pos = file_size - sizeof(ZipEOCD); pos >= search_start; pos--) {
        fseek(f, pos, SEEK_SET);
        uint32_t sig;
        if (fread(&sig, 4, 1, f) != 1) continue;
        if (sig == 0x06054b50) {
            ZipEOCD eocd;
            fseek(f, pos, SEEK_SET);
            if (fread(&eocd, sizeof(eocd), 1, f) == 1) {
                *cd_offset = eocd.cd_offset;
                *cd_count = eocd.cd_entries_total;
                return 0;
            }
        }
    }
    return -1;
}
```

### 步骤 3：添加 HTML 标签剥离

```c
/* HTML 标签剥离：提取纯文本 */
static char *strip_html(const char *html, size_t len)
{
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    size_t oi = 0;
    int in_tag = 0;
    int in_entity = 0;

    for (size_t i = 0; i < len; i++) {
        if (html[i] == '<') {
            in_tag = 1;
            /* 换行标签 → 换行 */
            if (i + 3 < len && html[i+1] == 'b' && html[i+2] == 'r' &&
                (html[i+3] == '>' || html[i+3] == ' ' || html[i+3] == '/')) {
                out[oi++] = '\n';
            }
            /* 段落标签 → 换行 */
            if (i + 2 < len && html[i+1] == 'p' &&
                (html[i+2] == '>' || html[i+2] == ' ')) {
                out[oi++] = '\n';
            }
            continue;
        }
        if (html[i] == '>') {
            in_tag = 0;
            continue;
        }
        if (in_tag) continue;

        /* HTML 实体 */
        if (html[i] == '&') {
            if (i + 5 < len && memcmp(html + i, "&amp;", 5) == 0) {
                out[oi++] = '&'; i += 4; continue;
            }
            if (i + 4 < len && memcmp(html + i, "&lt;", 4) == 0) {
                out[oi++] = '<'; i += 3; continue;
            }
            if (i + 4 < len && memcmp(html + i, "&gt;", 4) == 0) {
                out[oi++] = '>'; i += 3; continue;
            }
            if (i + 6 < len && memcmp(html + i, "&nbsp;", 6) == 0) {
                out[oi++] = ' '; i += 5; continue;
            }
            if (i + 6 < len && memcmp(html + i, "&quot;", 6) == 0) {
                out[oi++] = '"'; i += 5; continue;
            }
        }

        /* 跳过多余空白 */
        if (html[i] == ' ' || html[i] == '\t' || html[i] == '\r') {
            if (oi > 0 && out[oi-1] != '\n' && out[oi-1] != ' ') {
                out[oi++] = ' ';
            }
            continue;
        }

        out[oi++] = html[i];
    }
    out[oi] = '\0';

    /* 压缩连续空行 */
    char *result = (char *)realloc(out, oi + 1);
    return result ? result : out;
}
```

### 步骤 4：添加 EPUB 解析主函数

```c
/* 从 EPUB 路径中解析 OPF 容器 */
static char *epub_find_opf_path(FILE *f, long cd_offset, int cd_count)
{
    /* 查找 META-INF/container.xml */
    for (int i = 0; i < cd_count; i++) {
        fseek(f, cd_offset + i * sizeof(ZipCentralDirEntry), SEEK_SET);
        ZipCentralDirEntry entry;
        if (fread(&entry, sizeof(entry), 1, f) != 1) continue;

        char filename[256] = {0};
        long pos = ftell(f);
        fread(filename, 1, entry.filename_len < 255 ? entry.filename_len : 255, f);

        if (strcmp(filename, "META-INF/container.xml") == 0) {
            size_t data_size;
            void *data = zip_read_stored(f, &entry, &data_size);
            if (!data) return NULL;

            /* 简单解析：查找 rootfile full-path="..." */
            char *full_path = strstr((char *)data, "full-path=\"");
            if (full_path) {
                full_path += 11; /* skip full-path=" */
                char *end = strchr(full_path, '"');
                if (end) {
                    size_t len = end - full_path;
                    char *result = (char *)malloc(len + 1);
                    memcpy(result, full_path, len);
                    result[len] = '\0';
                    free(data);
                    return result;
                }
            }
            free(data);
        }
    }
    return NULL;
}

/* 从 OPF 中提取 spine（阅读顺序）并拼接文本 */
static char *epub_extract_text(FILE *f, long cd_offset, int cd_count, const char *opf_path)
{
    /* 找到 OPF 文件在 ZIP 中的位置 */
    ZipCentralDirEntry opf_entry;
    int found = 0;

    for (int i = 0; i < cd_count; i++) {
        fseek(f, cd_offset + i * sizeof(ZipCentralDirEntry), SEEK_SET);
        ZipCentralDirEntry entry;
        if (fread(&entry, sizeof(entry), 1, f) != 1) continue;

        char filename[256] = {0};
        fread(filename, 1, entry.filename_len < 255 ? entry.filename_len : 255, f);

        if (strcmp(filename, opf_path) == 0) {
            opf_entry = entry;
            found = 1;
            break;
        }
    }

    if (!found) return NULL;

    /* 读取 OPF 内容 */
    size_t opf_size;
    char *opf_data = (char *)zip_read_stored(f, &opf_entry, &opf_size);
    if (!opf_data) return NULL;

    /* 提取 OPF 所在目录（用于解析相对路径） */
    char opf_dir[256] = "";
    const char *last_slash = strrchr(opf_path, '/');
    if (last_slash) {
        size_t dir_len = last_slash - opf_path + 1;
        memcpy(opf_dir, opf_path, dir_len);
        opf_dir[dir_len] = '\0';
    }

    /* 简单解析 spine：提取所有 itemref idref */
    /* 先收集 manifest 中 id → href 的映射 */
    typedef struct {
        char id[64];
        char href[256];
    } ManifestItem;

    #define MAX_MANIFEST 256
    ManifestItem manifest[MAX_MANIFEST];
    int manifest_count = 0;

    /* 简单解析 manifest */
    char *pos = opf_data;
    while ((pos = strstr(pos, "<item ")) != NULL && manifest_count < MAX_MANIFEST) {
        char *id_start = strstr(pos, "id=\"");
        char *href_start = strstr(pos, "href=\"");
        if (id_start && href_start) {
            id_start += 4;
            href_start += 6;
            char *id_end = strchr(id_start, '"');
            char *href_end = strchr(href_start, '"');
            if (id_end && href_end) {
                size_t id_len = id_end - id_start;
                size_t href_len = href_end - href_start;
                if (id_len < 64 && href_len < 256) {
                    memcpy(manifest[manifest_count].id, id_start, id_len);
                    manifest[manifest_count].id[id_len] = '\0';
                    memcpy(manifest[manifest_count].href, href_start, href_len);
                    manifest[manifest_count].href[href_len] = '\0';
                    manifest_count++;
                }
            }
        }
        pos++;
    }

    /* 解析 spine：提取 idref 顺序 */
    char *spine_ids[256];
    int spine_count = 0;
    pos = opf_data;
    while ((pos = strstr(pos, "<itemref ")) != NULL && spine_count < 256) {
        char *ref_start = strstr(pos, "idref=\"");
        if (ref_start) {
            ref_start += 7;
            char *ref_end = strchr(ref_start, '"');
            if (ref_end) {
                size_t len = ref_end - ref_start;
                spine_ids[spine_count] = (char *)malloc(len + 1);
                memcpy(spine_ids[spine_count], ref_start, len);
                spine_ids[spine_count][len] = '\0';
                spine_count++;
            }
        }
        pos++;
    }

    /* 按 spine 顺序提取并拼接文本 */
    size_t total_size = 0;
    size_t total_cap = 64 * 1024;
    char *result = (char *)malloc(total_cap);
    if (!result) { free(opf_data); return NULL; }
    result[0] = '\0';

    for (int s = 0; s < spine_count; s++) {
        /* 查找对应 href */
        char *href = NULL;
        for (int m = 0; m < manifest_count; m++) {
            if (strcmp(manifest[m].id, spine_ids[s]) == 0) {
                href = manifest[m].href;
                break;
            }
        }
        if (!href) continue;

        /* 构建完整路径 */
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", opf_dir, href);

        /* 在 ZIP 中查找该文件 */
        for (int i = 0; i < cd_count; i++) {
            fseek(f, cd_offset + i * sizeof(ZipCentralDirEntry), SEEK_SET);
            ZipCentralDirEntry entry;
            if (fread(&entry, sizeof(entry), 1, f) != 1) continue;

            char filename[256] = {0};
            fread(filename, 1, entry.filename_len < 255 ? entry.filename_len : 255, f);

            if (strcmp(filename, full_path) == 0) {
                size_t data_size;
                void *data = zip_read_stored(f, &entry, &data_size);
                if (data) {
                    char *text = strip_html((char *)data, data_size);
                    if (text) {
                        size_t text_len = strlen(text);
                        if (total_size + text_len + 2 > total_cap) {
                            total_cap = (total_size + text_len + 2) * 2;
                            result = (char *)realloc(result, total_cap);
                        }
                        memcpy(result + total_size, text, text_len);
                        total_size += text_len;
                        result[total_size++] = '\n';
                        result[total_size] = '\0';
                        free(text);
                    }
                    free(data);
                }
                break;
            }
        }
        free(spine_ids[s]);
    }

    free(opf_data);
    return result;
}
```

### 步骤 5：更新 book_parser_load_text 支持 EPUB

在 `book_parser_load_text` 函数中，添加 EPUB 分支：

```c
char *book_parser_load_text(const char *path)
{
    if (!path) return NULL;

    BookFormat fmt = book_parser_detect(path);

    if (fmt == BOOK_FMT_TXT) {
        /* ... 现有 TXT 逻辑 ... */
    } else if (fmt == BOOK_FMT_EPUB) {
        FILE *f = fopen(path, "rb");
        if (!f) return NULL;

        long cd_offset;
        int cd_count;
        if (zip_find_central_dir(f, &cd_offset, &cd_count) != 0) {
            fclose(f);
            return NULL;
        }

        char *opf_path = epub_find_opf_path(f, cd_offset, cd_count);
        if (!opf_path) { fclose(f); return NULL; }

        char *text = epub_extract_text(f, cd_offset, cd_count, opf_path);
        free(opf_path);
        fclose(f);
        return text;
    }

    return NULL;
}
```

### 步骤 6：编译验证

运行：`make clean && make 2>&1`
预期：编译通过。

### 步骤 7：Commit

```bash
git add -A
git commit -m "feat: book_parser 添加 EPUB 解析（stored ZIP）

- ZIP 中央目录解析
- container.xml 和 OPF 解析
- HTML 标签剥离状态机
- 按 spine 顺序拼接章节文本"
```

---

## 任务 3：book_storage — JSON 持久化

**文件：**
- 创建：`shared/storage/book_storage.h`
- 创建：`shared/storage/book_storage.c`

### 步骤 1：创建 book_storage.h

```c
/**
 * @file book_storage.h
 * @brief 统一 JSON 存储（进度 + 书签）
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BOOKMARK_MAX_PER_BOOK 20

/**
 * @brief 初始化存储（加载或创建 JSON 文件）
 */
void book_storage_init(void);

/**
 * @brief 保存阅读进度（内存）
 */
void book_storage_save_progress(const char *book_name, int page);

/**
 * @brief 加载阅读进度
 * @return 页码，-1 表示无记录
 */
int book_storage_load_progress(const char *book_name);

/**
 * @brief 添加书签（内存）
 */
void book_storage_add_bookmark(const char *book_name, int page);

/**
 * @brief 获取书签列表
 * @param pages 输出页码数组
 * @param max_count 数组大小
 * @return 实际书签数量
 */
int book_storage_get_bookmarks(const char *book_name, int *pages, int max_count);

/**
 * @brief 判断某页是否有书签
 */
int book_storage_has_bookmark(const char *book_name, int page);

/**
 * @brief 写入文件（退出时调用）
 */
void book_storage_save(void);

#ifdef __cplusplus
}
#endif
```

### 步骤 2：创建 book_storage.c

```c
/**
 * @file book_storage.c
 * @brief 统一 JSON 存储实现
 */
#ifdef PC_SIMULATION

#include "book_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define DATA_DIR "data"
#define DATA_FILE "data/reading_data.json"
#define MAX_BOOKS 100

typedef struct {
    char name[256];
    int page;
    time_t timestamp;
} ProgressEntry;

typedef struct {
    char name[256];
    int pages[BOOKMARK_MAX_PER_BOOK];
    time_t timestamps[BOOKMARK_MAX_PER_BOOK];
    int count;
} BookmarkEntry;

static ProgressEntry s_progress[MAX_BOOKS];
static int s_progress_count = 0;

static BookmarkEntry s_bookmarks[MAX_BOOKS];
static int s_bookmark_count = 0;

/* 查找或创建进度条目 */
static ProgressEntry *find_progress(const char *name)
{
    for (int i = 0; i < s_progress_count; i++) {
        if (strcmp(s_progress[i].name, name) == 0) return &s_progress[i];
    }
    if (s_progress_count < MAX_BOOKS) {
        ProgressEntry *e = &s_progress[s_progress_count++];
        strncpy(e->name, name, sizeof(e->name) - 1);
        e->page = -1;
        e->timestamp = 0;
        return e;
    }
    return NULL;
}

/* 查找或创建书签条目 */
static BookmarkEntry *find_bookmarks(const char *name)
{
    for (int i = 0; i < s_bookmark_count; i++) {
        if (strcmp(s_bookmarks[i].name, name) == 0) return &s_bookmarks[i];
    }
    if (s_bookmark_count < MAX_BOOKS) {
        BookmarkEntry *e = &s_bookmarks[s_bookmark_count++];
        strncpy(e->name, name, sizeof(e->name) - 1);
        e->count = 0;
        return e;
    }
    return NULL;
}

void book_storage_init(void)
{
    mkdir(DATA_DIR, 0755);

    FILE *f = fopen(DATA_FILE, "r");
    if (!f) {
        printf("[Book Storage] 无现有数据，创建新文件\n");
        return;
    }

    /* 简单 JSON 解析：逐行读取 */
    char line[1024];
    char current_section[32] = "";
    char current_book[256] = "";

    while (fgets(line, sizeof(line), f)) {
        /* 去掉换行 */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        /* 跳过空白 */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* 检测 section */
        if (strstr(p, "\"progress\"")) {
            strcpy(current_section, "progress");
            continue;
        }
        if (strstr(p, "\"bookmarks\"")) {
            strcpy(current_section, "bookmarks");
            continue;
        }

        /* 解析 book name */
        char *key_start = strchr(p, '"');
        if (key_start) {
            key_start++;
            char *key_end = strchr(key_start, '"');
            if (key_end) {
                size_t len = key_end - key_start;
                if (len < sizeof(current_book)) {
                    memcpy(current_book, key_start, len);
                    current_book[len] = '\0';
                }
            }
        }

        /* 解析进度值 */
        if (strcmp(current_section, "progress") == 0 && current_book[0]) {
            char *colon = strchr(p, ':');
            if (colon) {
                char *page_str = strstr(colon, "\"page\"");
                if (page_str) {
                    page_str = strchr(page_str + 6, ':');
                    if (page_str) {
                        int page = atoi(page_str + 1);
                        ProgressEntry *e = find_progress(current_book);
                        if (e) e->page = page;
                    }
                }
            }
        }

        /* 解析书签值 */
        if (strcmp(current_section, "bookmarks") == 0 && current_book[0]) {
            char *page_ptr = strstr(p, "\"page\"");
            if (page_ptr) {
                page_ptr = strchr(page_ptr + 6, ':');
                if (page_ptr) {
                    int page = atoi(page_ptr + 1);
                    BookmarkEntry *e = find_bookmarks(current_book);
                    if (e && e->count < BOOKMARK_MAX_PER_BOOK) {
                        e->pages[e->count] = page;
                        e->timestamps[e->count] = time(NULL);
                        e->count++;
                    }
                }
            }
        }
    }

    fclose(f);
    printf("[Book Storage] 加载 %d 个进度, %d 本书签\n",
           s_progress_count, s_bookmark_count);
}

void book_storage_save_progress(const char *book_name, int page)
{
    ProgressEntry *e = find_progress(book_name);
    if (e) {
        e->page = page;
        e->timestamp = time(NULL);
    }
}

int book_storage_load_progress(const char *book_name)
{
    for (int i = 0; i < s_progress_count; i++) {
        if (strcmp(s_progress[i].name, book_name) == 0) {
            return s_progress[i].page;
        }
    }
    return -1;
}

void book_storage_add_bookmark(const char *book_name, int page)
{
    BookmarkEntry *e = find_bookmarks(book_name);
    if (!e) return;

    /* 检查是否已存在 */
    for (int i = 0; i < e->count; i++) {
        if (e->pages[i] == page) return;
    }

    /* 超出限制时删除最早的 */
    if (e->count >= BOOKMARK_MAX_PER_BOOK) {
        memmove(e->pages, e->pages + 1, (BOOKMARK_MAX_PER_BOOK - 1) * sizeof(int));
        memmove(e->timestamps, e->timestamps + 1, (BOOKMARK_MAX_PER_BOOK - 1) * sizeof(time_t));
        e->count--;
    }

    e->pages[e->count] = page;
    e->timestamps[e->count] = time(NULL);
    e->count++;
}

int book_storage_get_bookmarks(const char *book_name, int *pages, int max_count)
{
    for (int i = 0; i < s_bookmark_count; i++) {
        if (strcmp(s_bookmarks[i].name, book_name) == 0) {
            int count = s_bookmarks[i].count < max_count ? s_bookmarks[i].count : max_count;
            memcpy(pages, s_bookmarks[i].pages, count * sizeof(int));
            return count;
        }
    }
    return 0;
}

int book_storage_has_bookmark(const char *book_name, int page)
{
    for (int i = 0; i < s_bookmark_count; i++) {
        if (strcmp(s_bookmarks[i].name, book_name) == 0) {
            for (int j = 0; j < s_bookmarks[i].count; j++) {
                if (s_bookmarks[i].pages[j] == page) return 1;
            }
        }
    }
    return 0;
}

void book_storage_save(void)
{
    FILE *f = fopen(DATA_FILE, "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"progress\": {\n");
    for (int i = 0; i < s_progress_count; i++) {
        if (s_progress[i].page >= 0) {
            fprintf(f, "    \"%s\": { \"page\": %d, \"timestamp\": %ld }%s\n",
                    s_progress[i].name, s_progress[i].page,
                    (long)s_progress[i].timestamp,
                    (i < s_progress_count - 1) ? "," : "");
        }
    }
    fprintf(f, "  },\n");

    fprintf(f, "  \"bookmarks\": {\n");
    for (int i = 0; i < s_bookmark_count; i++) {
        if (s_bookmarks[i].count > 0) {
            fprintf(f, "    \"%s\": [\n", s_bookmarks[i].name);
            for (int j = 0; j < s_bookmarks[i].count; j++) {
                fprintf(f, "      { \"page\": %d, \"timestamp\": %ld }%s\n",
                        s_bookmarks[i].pages[j],
                        (long)s_bookmarks[i].timestamps[j],
                        (j < s_bookmarks[i].count - 1) ? "," : "");
            }
            fprintf(f, "    ]%s\n", (i < s_bookmark_count - 1) ? "," : "");
        }
    }
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
    printf("[Book Storage] 数据已保存到 %s\n", DATA_FILE);
}

#endif /* PC_SIMULATION */
```

### 步骤 3：更新 Makefile

添加：
```
          shared/storage/book_storage.c \
```

### 步骤 4：编译验证

运行：`make clean && make 2>&1`
预期：编译通过。

### 步骤 5：Commit

```bash
git add -A
git commit -m "feat: 添加 book_storage JSON 持久化模块

- 阅读进度保存/加载
- 书签管理（每书最多 20 个）
- JSON 文件读写
- 退出时自动保存"
```

---

## 任务 4：mock_books 改造 — 本地目录扫描

**文件：**
- 修改：`shared/mock/mock_books.c`
- 修改：`shared/mock/mock_books.h`

### 步骤 1：添加 book_parser 头文件引用

在 `mock_books.c` 顶部添加：
```c
#include "../engine/book_parser.h"
#include "../storage/book_storage.h"
#include <dirent.h>
```

### 步骤 2：添加文件扫描和文本加载接口

在 `mock_books.h` 中添加：
```c
/**
 * @brief 获取书籍文件路径
 * @param index 索引
 * @param path 输出缓冲区
 * @param size 缓冲区大小
 * @return 0 成功
 */
int mock_books_get_path(int index, char *path, int size);

/**
 * @brief 加载书籍文本（按需，调用者 free）
 * @param index 索引
 * @return UTF-8 文本，NULL 失败
 */
char *mock_books_load_text(int index);

/**
 * @brief 获取书籍文件名（用于存储 key）
 * @param index 索引
 * @return 文件名字符串
 */
const char *mock_books_get_filename(int index);
```

### 步骤 3：重写 mock_books.c

```c
/**
 * @file mock_books.c
 * @brief Mock 书籍模块 - 从本地目录扫描书籍
 */
#ifdef PC_SIMULATION

#include "mock_books.h"
#include "../engine/book_parser.h"
#include "../storage/book_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#define BOOKS_DIR "shared/books"
#define MAX_BOOKS 50

static MockBookMeta s_books[MAX_BOOKS];
static int s_book_count = 0;

/* 模拟统计 */
static uint32_t s_today_minutes = 38;
static uint32_t s_total_hours = 127;

void mock_books_init(void)
{
    s_book_count = 0;

    DIR *dir = opendir(BOOKS_DIR);
    if (!dir) {
        printf("[Mock Books] 书籍目录不存在: %s\n", BOOKS_DIR);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_book_count < MAX_BOOKS) {
        if (entry->d_name[0] == '.') continue;

        /* 检查文件扩展名 */
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext) continue;
        if (strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".epub") != 0) continue;

        /* 构建完整路径 */
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", BOOKS_DIR, entry->d_name);

        /* 提取元数据 */
        BookInfo info;
        if (book_parser_extract_info(path, &info) == 0) {
            MockBookMeta *book = &s_books[s_book_count];
            snprintf(book->id, sizeof(book->id), "book_%03d", s_book_count);
            strncpy(book->title, info.title, sizeof(book->title) - 1);
            strncpy(book->author, info.author, sizeof(book->author) - 1);
            strncpy(book->filePath, path, sizeof(book->filePath) - 1);
            book->totalPages = info.total_chars > 0 ? (info.total_chars / 500) + 1 : 1;
            book->currentPage = 0;
            book->readingSeconds = 0;
            book->isCurrentReading = (s_book_count == 0);

            /* 从存储加载进度 */
            int saved_page = book_storage_load_progress(entry->d_name);
            if (saved_page >= 0) {
                book->currentPage = saved_page;
            }

            s_book_count++;
        }
    }

    closedir(dir);
    printf("[Mock Books] 扫描完成，共 %d 本书\n", s_book_count);
}

int mock_books_get_count(void)
{
    return s_book_count;
}

int mock_books_get_by_index(int index, MockBookMeta *book)
{
    if (index < 0 || index >= s_book_count || !book) return -1;
    *book = s_books[index];
    return 0;
}

int mock_books_get_path(int index, char *path, int size)
{
    if (index < 0 || index >= s_book_count || !path) return -1;
    strncpy(path, s_books[index].filePath, size - 1);
    path[size - 1] = '\0';
    return 0;
}

char *mock_books_load_text(int index)
{
    if (index < 0 || index >= s_book_count) return NULL;
    return book_parser_load_text(s_books[index].filePath);
}

const char *mock_books_get_filename(int index)
{
    if (index < 0 || index >= s_book_count) return NULL;
    const char *name = strrchr(s_books[index].filePath, '/');
    return name ? name + 1 : s_books[index].filePath;
}

int mock_books_get_current_reading(MockBookMeta *book)
{
    for (int i = 0; i < s_book_count; i++) {
        if (s_books[i].isCurrentReading) {
            if (book) *book = s_books[i];
            return 0;
        }
    }
    return -1;
}

void mock_books_get_count_str(char *buf, int size)
{
    snprintf(buf, size, "藏书: %d 册", s_book_count);
}

void mock_books_get_today_reading_str(char *buf, int size)
{
    snprintf(buf, size, "今日阅读: %lu 分钟", (unsigned long)s_today_minutes);
}

void mock_books_get_total_reading_str(char *buf, int size)
{
    snprintf(buf, size, "累计阅读: %lu 小时", (unsigned long)s_total_hours);
}

void mock_books_get_current_book_str(char *buf, int size)
{
    MockBookMeta book;
    if (mock_books_get_current_reading(&book) == 0) {
        snprintf(buf, size, "正在读: %s", book.title);
        if (strlen(buf) > 28) {
            buf[25] = '.';
            buf[26] = '.';
            buf[27] = '.';
            buf[28] = '\0';
        }
    } else {
        snprintf(buf, size, "正在读: 无");
    }
}

void mock_books_get_progress_str(char *buf, int size)
{
    MockBookMeta book;
    if (mock_books_get_current_reading(&book) == 0 && book.totalPages > 0) {
        int pct = (book.currentPage * 100) / book.totalPages;
        snprintf(buf, size, "  第 %lu/%lu 页 (%d%%)",
                 (unsigned long)(book.currentPage + 1),
                 (unsigned long)book.totalPages, pct);
    } else {
        snprintf(buf, size, "  无进度");
    }
}

int mock_books_get_progress_percent(void)
{
    MockBookMeta book;
    if (mock_books_get_current_reading(&book) == 0 && book.totalPages > 0) {
        return (book.currentPage * 100) / book.totalPages;
    }
    return 0;
}

#endif /* PC_SIMULATION */
```

### 步骤 4：创建测试书籍目录和文件

```bash
mkdir -p shared/books
```

创建一个简单的测试 TXT 文件 `shared/books/测试书籍.txt`（包含几行中文文本）。

### 步骤 5：更新 sim_main.c 初始化顺序

在 `sim_main.c` 的 `main()` 函数中，在 `mock_books_init()` 之前添加：
```c
book_storage_init();
```

添加头文件引用：
```c
#include "shared/storage/book_storage.h"
```

### 步骤 6：编译验证

运行：`make clean && make 2>&1`
预期：编译通过，输出显示扫描到书籍。

### 步骤 7：Commit

```bash
git add -A
git commit -m "feat: mock_books 改造为本地目录扫描

- 扫描 shared/books/ 目录
- 使用 book_parser 提取元数据
- 集成 book_storage 加载进度
- 添加文本加载接口"
```

---

## 任务 5：设置页交互

**文件：**
- 修改：`shared/ui/page_settings.c`

### 步骤 1：添加设置项数据结构和状态变量

在 `page_settings.c` 顶部添加：

```c
#include "../storage/book_storage.h"

typedef struct {
    const char *label;
    const char *options[8];
    int option_count;
    int current_value;
    int edit_value;  /* 编辑中的临时值 */
} SettingItem;

/* 阅读排版设置 */
static const char *font_size_opts[] = {"16px", "18px", "20px", "22px", "24px", "28px", "32px"};
static const char *line_spacing_opts[] = {"1.0", "1.2", "1.5", "2.0"};
static const char *char_spacing_opts[] = {"0px", "1px", "2px", "3px", "4px"};
static const char *margin_opts[] = {"4px", "8px", "12px", "16px"};

/* 系统设置 */
static const char *sleep_timeout_opts[] = {"5分钟", "10分钟", "30分钟", "不自动"};

static SettingItem s_settings[] = {
    { "字号",     {0}, 7, 2, 2 },  /* 默认 20px */
    { "行间距",   {0}, 4, 2, 2 },  /* 默认 1.5 */
    { "字间距",   {0}, 5, 1, 1 },  /* 默认 1px */
    { "页边距",   {0}, 4, 1, 1 },  /* 默认 8px */
    { "自动休眠", {0}, 4, 0, 0 },  /* 默认 5分钟 */
};
#define SETTING_COUNT (sizeof(s_settings) / sizeof(s_settings[0]))

static int s_cursor = 0;        /* 光标位置 */
static int s_editing = 0;       /* 是否在编辑模式 */

/* 分类标签（在设置项数组中的索引位置之前） */
static const char *s_section_labels[] = { "阅读排版", "系统设置" };
static int s_section_indices[] = { 0, 4 };  /* 每个分类的起始索引 */
```

### 步骤 2：重写 on_render 函数

```c
static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int y = 28;
    int line_h = 22;
    int setting_idx = 0;

    /* 阅读排版 */
    widget_draw_text(20, y, "阅读排版", RENDERER_COLOR_RED);
    y += line_h;
    renderer_fill_rect(20, y, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
    y += 6;

    for (int i = 0; i < 4; i++, setting_idx++) {
        SettingItem *s = &s_settings[setting_idx];

        /* 光标指示 */
        if (s_cursor == setting_idx) {
            widget_draw_text(20, y, "▶", s_editing ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK);
        }

        /* 标签 */
        widget_draw_text(38, y, s->label, RENDERER_COLOR_BLACK);

        /* 值 */
        char val_buf[32];
        int val_idx = s_editing && s_cursor == setting_idx ? s->edit_value : s->current_value;
        snprintf(val_buf, sizeof(val_buf), "%s", s->options[val_idx]);
        int val_x = 180;
        RendererColor val_color = (s_editing && s_cursor == setting_idx) ?
                                  RENDERER_COLOR_RED : RENDERER_COLOR_BLACK;
        widget_draw_text(val_x, y, val_buf, val_color);

        y += line_h;
    }

    y += 8;
    renderer_fill_rect(20, y, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
    y += 6;

    /* 系统设置 */
    widget_draw_text(20, y, "系统设置", RENDERER_COLOR_RED);
    y += line_h;
    renderer_fill_rect(20, y, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
    y += 6;

    for (int i = 0; i < 1; i++, setting_idx++) {
        SettingItem *s = &s_settings[setting_idx];

        if (s_cursor == setting_idx) {
            widget_draw_text(20, y, "▶", s_editing ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK);
        }

        widget_draw_text(38, y, s->label, RENDERER_COLOR_BLACK);

        char val_buf[32];
        int val_idx = s_editing && s_cursor == setting_idx ? s->edit_value : s->current_value;
        snprintf(val_buf, sizeof(val_buf), "%s", s->options[val_idx]);
        int val_x = 180;
        RendererColor val_color = (s_editing && s_cursor == setting_idx) ?
                                  RENDERER_COLOR_RED : RENDERER_COLOR_BLACK;
        widget_draw_text(val_x, y, val_buf, val_color);

        y += line_h;
    }

    /* 版本信息 */
    y += 8;
    widget_draw_text(20, y, "版本: v0.5.0", RENDERER_COLOR_BLACK);

    /* 底部提示 */
    if (s_editing) {
        widget_draw_text(20, 280, "[PREV/NEXT] 切换  [HOME] 确认  [ESC] 取消", RENDERER_COLOR_BLACK);
    } else {
        widget_draw_text(20, 280, "[PREV/NEXT] 移动  [HOME] 编辑  [ESC] 返回", RENDERER_COLOR_BLACK);
    }
}
```

### 步骤 3：重写 on_key 函数

```c
static void on_enter(void)
{
    printf("[设置] 进入\n");
    s_cursor = 0;
    s_editing = 0;
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    if (s_editing) {
        /* 编辑模式 */
        switch (key) {
        case KEY_PREV:
            s_settings[s_cursor].edit_value--;
            if (s_settings[s_cursor].edit_value < 0) {
                s_settings[s_cursor].edit_value = s_settings[s_cursor].option_count - 1;
            }
            break;
        case KEY_NEXT:
            s_settings[s_cursor].edit_value++;
            if (s_settings[s_cursor].edit_value >= s_settings[s_cursor].option_count) {
                s_settings[s_cursor].edit_value = 0;
            }
            break;
        case KEY_HOME:
            /* 确认修改 */
            s_settings[s_cursor].current_value = s_settings[s_cursor].edit_value;
            s_editing = 0;
            printf("[设置] 确认: %s = %s\n",
                   s_settings[s_cursor].label,
                   s_settings[s_cursor].options[s_settings[s_cursor].current_value]);
            break;
        case KEY_PWR:
            /* 取消修改 */
            s_settings[s_cursor].edit_value = s_settings[s_cursor].current_value;
            s_editing = 0;
            break;
        default:
            break;
        }
    } else {
        /* 浏览模式 */
        switch (key) {
        case KEY_PREV:
            if (s_cursor > 0) s_cursor--;
            break;
        case KEY_NEXT:
            if (s_cursor < (int)SETTING_COUNT - 1) s_cursor++;
            break;
        case KEY_HOME:
            /* 进入编辑模式 */
            s_settings[s_cursor].edit_value = s_settings[s_cursor].current_value;
            s_editing = 1;
            break;
        case KEY_PWR:
            page_mgr_switch(PAGE_HOME);
            break;
        default:
            break;
        }
    }

    on_render();
}
```

### 步骤 4：初始化设置选项字符串数组

在 `on_enter` 之前添加初始化函数：

```c
static void settings_init(void)
{
    /* 设置选项字符串 */
    for (int i = 0; i < 7; i++) s_settings[0].options[i] = font_size_opts[i];
    for (int i = 0; i < 4; i++) s_settings[1].options[i] = line_spacing_opts[i];
    for (int i = 0; i < 5; i++) s_settings[2].options[i] = char_spacing_opts[i];
    for (int i = 0; i < 4; i++) s_settings[3].options[i] = margin_opts[i];
    for (int i = 0; i < 4; i++) s_settings[4].options[i] = sleep_timeout_opts[i];
}
```

在 `on_enter` 中调用 `settings_init()`。

### 步骤 5：编译验证

运行：`make clean && make 2>&1`
预期：编译通过。

### 步骤 6：Commit

```bash
git add -A
git commit -m "feat: 设置页交互化

- BROWSE/EDIT 双模式状态机
- PREV/NEXT 移动光标 / 切换值
- HOME 确认编辑 / PWR 取消
- 字号/行距/字距/页边距/休眠超时设置"
```

---

## 任务 6：阅读上下文菜单 + 进度持久化

**文件：**
- 修改：`shared/ui/page_reader.c`

### 步骤 1：添加头文件和状态变量

```c
#include "page_reader.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include "../mock/mock_books.h"
#include "../storage/book_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 阅读状态 */
static int s_book_index = -1;       /* 当前书籍索引 */
static char *s_book_text = NULL;    /* 加载的文本 */
static int s_current_page = 0;
static int s_total_pages = 1;
static char s_book_name[256] = "";  /* 书籍文件名（用于存储 key） */

/* 分页参数 */
#define PAGE_LINES 14
#define LINE_HEIGHT 18
#define MARGIN_X 20
#define MARGIN_Y 25

/* 每页文本行 */
static char s_page_lines[PAGE_LINES + 1][128];

/* 上下文菜单状态 */
static int s_menu_visible = 0;
static int s_menu_cursor = 0;
static const char *s_menu_items[] = {
    "▶ 继续阅读",
    "★ 添加书签",
    "ℹ 书籍信息",
};
#define MENU_COUNT (sizeof(s_menu_items) / sizeof(s_menu_items[0]))

/* 书籍信息显示状态 */
static int s_show_info = 0;
```

### 步骤 2：添加分页函数

```c
/* 计算总页数 */
static void calculate_pages(void)
{
    if (!s_book_text) { s_total_pages = 1; return; }

    int line_count = 0;
    int page_count = 1;
    const char *p = s_book_text;

    while (*p) {
        if (*p == '\n') {
            line_count++;
            if (line_count >= PAGE_LINES) {
                page_count++;
                line_count = 0;
            }
        }
        p++;
    }

    s_total_pages = page_count > 0 ? page_count : 1;
}

/* 获取指定页的文本行 */
static void load_page_lines(int page)
{
    /* 清空 */
    for (int i = 0; i <= PAGE_LINES; i++) {
        s_page_lines[i][0] = '\0';
    }

    if (!s_book_text) return;

    /* 跳到目标页 */
    int line_count = 0;
    int current_page = 0;
    const char *p = s_book_text;

    while (*p && current_page < page) {
        if (*p == '\n') {
            line_count++;
            if (line_count >= PAGE_LINES) {
                current_page++;
                line_count = 0;
            }
        }
        p++;
    }

    /* 读取当前页的行 */
    int line_idx = 0;
    while (*p && line_idx < PAGE_LINES) {
        int char_count = 0;
        while (*p && *p != '\n' && char_count < 120) {
            s_page_lines[line_idx][char_count++] = *p;
            p++;
        }
        s_page_lines[line_idx][char_count] = '\0';
        line_idx++;

        if (*p == '\n') p++;
    }
}
```

### 步骤 3：重写 on_enter 和 on_exit

```c
static void on_enter(void)
{
    printf("[阅读器] 进入\n");
    s_menu_visible = 0;
    s_show_info = 0;

    /* 从书架获取当前选中的书 */
    /* 注意：sim_main.c 中 s_selected_book 是当前选中索引 */
    /* 这里通过 mock_books 接口获取 */
    MockBookMeta book;
    if (mock_books_get_current_reading(&book) == 0) {
        s_book_index = 0; /* 默认第一本 */
        strncpy(s_book_name, mock_books_get_filename(s_book_index), sizeof(s_book_name) - 1);
    }

    /* 加载文本 */
    if (s_book_text) {
        free(s_book_text);
        s_book_text = NULL;
    }

    if (s_book_index >= 0) {
        s_book_text = mock_books_load_text(s_book_index);
        if (s_book_text) {
            calculate_pages();

            /* 恢复进度 */
            int saved = book_storage_load_progress(s_book_name);
            if (saved >= 0 && saved < s_total_pages) {
                s_current_page = saved;
            } else {
                s_current_page = 0;
            }

            load_page_lines(s_current_page);
        }
    }
}

static void on_exit(void)
{
    /* 保存进度 */
    if (s_book_name[0] && s_current_page >= 0) {
        book_storage_save_progress(s_book_name, s_current_page);
        book_storage_save();
    }

    if (s_book_text) {
        free(s_book_text);
        s_book_text = NULL;
    }
}
```

### 步骤 4：重写 on_render 函数

```c
static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int y = MARGIN_Y;

    /* 正文 */
    for (int i = 0; i < PAGE_LINES; i++) {
        if (s_page_lines[i][0] != '\0') {
            widget_draw_text(MARGIN_X, y, s_page_lines[i], RENDERER_COLOR_BLACK);
        }
        y += LINE_HEIGHT;
    }

    /* 进度条 */
    int bar_x = 20, bar_y = 275, bar_w = RENDERER_WIDTH - 40;
    int progress = (s_total_pages > 0) ? (s_current_page * 100) / s_total_pages : 0;

    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 6, RENDERER_COLOR_BLACK);

    char page_str[32];
    snprintf(page_str, sizeof(page_str), "第 %d/%d 页 (%d%%)",
             s_current_page + 1, s_total_pages, progress);
    int text_w = widget_text_width(page_str);
    widget_draw_text((RENDERER_WIDTH - text_w) / 2, bar_y + 14, page_str, RENDERER_COLOR_BLACK);

    /* 上下文菜单覆盖层 */
    if (s_menu_visible) {
        int menu_x = 80, menu_y = 60, menu_w = 240, menu_h = 30 + MENU_COUNT * 22;
        renderer_fill_rect(menu_x, menu_y, menu_w, menu_h, RENDERER_COLOR_WHITE);
        renderer_draw_rect(menu_x, menu_y, menu_w, menu_h, RENDERER_COLOR_BLACK);

        for (int i = 0; i < (int)MENU_COUNT; i++) {
            RendererColor color = (i == s_menu_cursor) ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK;
            widget_draw_text(menu_x + 15, menu_y + 12 + i * 22, s_menu_items[i], color);
        }
    }

    /* 书籍信息覆盖层 */
    if (s_show_info) {
        int info_x = 60, info_y = 50, info_w = 280, info_h = 120;
        renderer_fill_rect(info_x, info_y, info_w, info_h, RENDERER_COLOR_WHITE);
        renderer_draw_rect(info_x, info_y, info_w, info_h, RENDERER_COLOR_BLACK);

        MockBookMeta book;
        if (s_book_index >= 0 && mock_books_get_by_index(s_book_index, &book) == 0) {
            char info_buf[128];
            snprintf(info_buf, sizeof(info_buf), "书名: %s", book.title);
            widget_draw_text(info_x + 15, info_y + 15, info_buf, RENDERER_COLOR_BLACK);

            snprintf(info_buf, sizeof(info_buf), "总页数: %d", s_total_pages);
            widget_draw_text(info_x + 15, info_y + 37, info_buf, RENDERER_COLOR_BLACK);

            snprintf(info_buf, sizeof(info_buf), "当前: 第 %d 页 (%d%%)",
                     s_current_page + 1, progress);
            widget_draw_text(info_x + 15, info_y + 59, info_buf, RENDERER_COLOR_BLACK);

            /* 书签数 */
            int bm_pages[20];
            int bm_count = book_storage_get_bookmarks(s_book_name, bm_pages, 20);
            snprintf(info_buf, sizeof(info_buf), "书签: %d 个", bm_count);
            widget_draw_text(info_x + 15, info_y + 81, info_buf, RENDERER_COLOR_BLACK);
        }

        widget_draw_text(info_x + 15, info_y + 100, "[HOME] 关闭", RENDERER_COLOR_BLACK);
    }
}
```

### 步骤 5：重写 on_key 函数

```c
static void on_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    /* 书籍信息显示中 */
    if (s_show_info) {
        if (key == KEY_HOME || key == KEY_PWR) {
            s_show_info = 0;
            on_render();
        }
        return;
    }

    /* 菜单显示中 */
    if (s_menu_visible) {
        switch (key) {
        case KEY_PREV:
            if (s_menu_cursor > 0) s_menu_cursor--;
            on_render();
            break;
        case KEY_NEXT:
            if (s_menu_cursor < (int)MENU_COUNT - 1) s_menu_cursor++;
            on_render();
            break;
        case KEY_HOME:
            /* 确认菜单项 */
            switch (s_menu_cursor) {
            case 0: /* 继续阅读 */
                s_menu_visible = 0;
                break;
            case 1: /* 添加书签 */
                book_storage_add_bookmark(s_book_name, s_current_page);
                book_storage_save();
                s_menu_visible = 0;
                printf("[阅读器] 添加书签: %s 第 %d 页\n", s_book_name, s_current_page);
                break;
            case 2: /* 书籍信息 */
                s_menu_visible = 0;
                s_show_info = 1;
                break;
            }
            on_render();
            break;
        case KEY_PWR:
            /* 关闭菜单 */
            s_menu_visible = 0;
            on_render();
            break;
        default:
            break;
        }
        return;
    }

    /* 正常阅读模式 */
    switch (key) {
    case KEY_PREV:
        if (s_current_page > 0) {
            s_current_page--;
            load_page_lines(s_current_page);
        }
        on_render();
        break;
    case KEY_NEXT:
        if (s_current_page < s_total_pages - 1) {
            s_current_page++;
            load_page_lines(s_current_page);
        }
        on_render();
        break;
    case KEY_HOME:
        /* 弹出上下文菜单 */
        s_menu_visible = 1;
        s_menu_cursor = 0;
        on_render();
        break;
    case KEY_PWR:
        /* 返回书架 */
        page_mgr_switch(PAGE_BOOKSHELF);
        break;
    default:
        break;
    }
}
```

### 步骤 6：更新 vtbl

```c
const PageVtbl page_reader_vtbl = {
    .id = PAGE_READER,
    .on_enter = on_enter,
    .on_exit = on_exit,
    .on_key = on_key,
    .on_render = on_render,
};
```

### 步骤 7：编译验证

运行：`make clean && make 2>&1`
预期：编译通过。

### 步骤 8：Commit

```bash
git add -A
git commit -m "feat: 阅读器上下文菜单 + 进度持久化

- HOME 键弹出上下文菜单（继续/书签/信息）
- 翻页自动保存进度
- 退出自动写入文件
- 打开书自动恢复位置
- 书籍信息弹窗显示"
```

---

## 任务 7：书架页集成真实书籍

**文件：**
- 修改：`shared/ui/page_bookshelf.c`

### 步骤 1：添加 mock_books 引用

```c
#include "page_bookshelf.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include "../mock/mock_books.h"
#include <stdio.h>

static int s_selected = 0;
static int s_book_count = 0;
```

### 步骤 2：重写 on_enter 和 on_render

```c
static void on_enter(void)
{
    printf("[书架] 进入\n");
    s_book_count = mock_books_get_count();
    if (s_selected >= s_book_count) s_selected = 0;
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    if (s_book_count == 0) {
        widget_draw_text(20, 120, "暂无书籍", RENDERER_COLOR_BLACK);
        widget_draw_text(20, 140, "请将 .txt/.epub 文件放入", RENDERER_COLOR_BLACK);
        widget_draw_text(20, 158, "shared/books/ 目录", RENDERER_COLOR_BLACK);
        return;
    }

    /* 绘制 3x2 网格 */
    int cols = 3, rows = 2;
    for (int i = 0; i < 6 && i < s_book_count; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = 20 + col * 120;
        int y = 30 + row * 100;

        /* 封面 */
        renderer_draw_rect(x, y, 100, 80, RENDERER_COLOR_BLACK);

        /* 书本图标 */
        renderer_fill_rect(x + 30, y + 15, 40, 50, RENDERER_COLOR_BLACK);
        renderer_fill_rect(x + 32, y + 17, 36, 46, RENDERER_COLOR_WHITE);

        /* 选中框 */
        if (i == s_selected) {
            renderer_draw_rect(x - 2, y - 2, 104, 84, RENDERER_COLOR_RED);
        }

        /* 书名 */
        MockBookMeta book;
        if (mock_books_get_by_index(i, &book) == 0) {
            char title[16];
            strncpy(title, book.title, sizeof(title) - 1);
            title[sizeof(title) - 1] = '\0';
            int tw = widget_text_width(title);
            int tx = x + (100 - tw) / 2;
            widget_draw_text(tx, y + 82, title, RENDERER_COLOR_BLACK);
        }
    }

    /* 翻页指示 */
    char info[64];
    snprintf(info, sizeof(info), "共 %d 本", s_book_count);
    widget_draw_text(20, 260, info, RENDERER_COLOR_BLACK);

    /* 提示 */
    widget_draw_text(20, 280, "[HOME] 阅读  [PREV/NEXT] 选择", RENDERER_COLOR_BLACK);
}
```

### 步骤 3：重写 on_key

```c
static void on_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    switch (key) {
    case KEY_PREV:
        if (s_selected > 0) s_selected--;
        break;
    case KEY_NEXT:
        if (s_selected < s_book_count - 1) s_selected++;
        break;
    case KEY_HOME:
        if (s_book_count > 0) {
            /* 设置当前阅读书籍 */
            MockBookMeta book;
            if (mock_books_get_by_index(s_selected, &book) == 0) {
                /* 标记为当前阅读 */
                page_mgr_switch(PAGE_READER);
            }
        }
        break;
    case KEY_PWR:
        page_mgr_switch(PAGE_HOME);
        break;
    default:
        break;
    }

    on_render();
}
```

### 步骤 4：编译验证

运行：`make clean && make 2>&1`
预期：编译通过。

### 步骤 5：Commit

```bash
git add -A
git commit -m "feat: 书架页集成真实书籍列表

- 从 mock_books 获取书籍数据
- 显示真实书名
- 空书架提示
- 选中后进入阅读器"
```

---

## 任务 8：集成测试 + 创建测试书籍

**文件：**
- 创建：`shared/books/测试书籍.txt`
- 修改：`sim_main.c`（确保初始化顺序正确）

### 步骤 1：创建测试书籍

创建 `shared/books/测试书籍.txt`，包含几段中文文本（至少 30 行，用于测试分页）。

### 步骤 2：验证 sim_main.c 初始化顺序

确认 `book_storage_init()` 在 `mock_books_init()` 之前调用。

### 步骤 3：完整编译和运行测试

```bash
make clean && make 2>&1
./reader_sim
```

测试流程：
1. 启动 → 主页显示
2. 切换到书架 → 应显示扫描到的书籍
3. 选择书籍 → 进入阅读器
4. 翻几页 → 退出
5. 重新进入 → 应恢复到退出时的位置
6. 阅读中按 HOME → 弹出菜单
7. 添加书签 → 确认
8. 进入设置 → 测试交互

### 步骤 4：Commit

```bash
git add -A
git commit -m "test: 添加测试书籍，完成核心功能集成测试"
```

---

## 自检清单

- [ ] 规格覆盖度：6 个功能全部有对应任务
- [ ] 占位符扫描：无 TODO/待定
- [ ] 类型一致性：MockBookMeta、BookInfo、SettingItem 等类型在各任务中一致
- [ ] 文件路径：所有路径使用 `shared/` 前缀
- [ ] 编译验证：每个任务结束前编译通过
- [ ] Commit 频率：每个任务至少一次 commit

---

**计划版本**: v1.0
**状态**: 已完成，待执行

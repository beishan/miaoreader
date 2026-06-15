/**
 * @file book_parser.c
 * @brief 书籍文件解析：TXT（UTF-8/GBK 自动检测）+ EPUB（ZIP reader with deflate）
 */
#include "book_parser.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <cJSON.h>
#include "miniz.h"
#include "gbk_table.h"

static const char *TAG = "book_parser";

#define MAX_TXT_SIZE   (8  * 1024 * 1024)
#define MAX_EPUB_SIZE  (32 * 1024 * 1024)

/* 文件魔数：ZIP local file header = "PK\x03\x04" */
static const uint8_t ZIP_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};
static const uint8_t UTF8_BOM[]  = {0xEF, 0xBB, 0xBF};

/* 内存分配辅助：优先 PSRAM，fallback 到内部 SRAM */
static void *smart_malloc(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) return ptr;
    ESP_LOGW(TAG, "PSRAM 分配失败 (%u bytes)，fallback 到内部 SRAM", (unsigned)size);
    return malloc(size);
}

static void *smart_calloc(size_t count, size_t size)
{
    void *ptr = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM);
    if (ptr) return ptr;
    ESP_LOGW(TAG, "PSRAM calloc 失败 (%u bytes)，fallback 到内部 SRAM", (unsigned)(count * size));
    return calloc(count, size);
}

static void *smart_realloc(void *ptr, size_t size)
{
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
    if (new_ptr) return new_ptr;
    ESP_LOGW(TAG, "PSRAM realloc 失败 (%u bytes)，fallback 到内部 SRAM", (unsigned)size);
    return realloc(ptr, size);
}

BookFormat book_detect_format(const char *file_path)
{
    if (!file_path) return BOOK_UNKNOWN;
    /* 扩展名优先 */
    const char *ext = strrchr(file_path, '.');
    if (ext) {
        if (strcasecmp(ext, ".epub") == 0) return BOOK_EPUB;
        if (strcasecmp(ext, ".txt")  == 0) return BOOK_TXT;
    }
    /* 魔数检测 */
    FILE *f = fopen(file_path, "rb");
    if (!f) return BOOK_UNKNOWN;
    uint8_t head[4] = {0};
    fread(head, 1, 4, f);
    fclose(f);
    if (memcmp(head, ZIP_MAGIC, 4) == 0) return BOOK_EPUB;
    if (memcmp(head, UTF8_BOM, 3) == 0) return BOOK_TXT;
    return BOOK_TXT;
}

/* 判断 UTF-8 字节序列合法性（启发式：检查非 ASCII 字节是否为合法多字节首字节） */
static bool looks_like_utf8(const uint8_t *buf, size_t n)
{
    int multibyte = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t b = buf[i];
        if (multibyte > 0) {
            if ((b & 0xC0) != 0x80) return false;
            multibyte--;
            continue;
        }
        if (b < 0x80) continue;
        if ((b & 0xE0) == 0xC0) { multibyte = 1; continue; }
        if ((b & 0xF0) == 0xE0) { multibyte = 2; continue; }
        if ((b & 0xF8) == 0xF0) { multibyte = 3; continue; }
        return false;
    }
    return multibyte == 0;
}

/* GBK → UTF-8 转换（使用精确映射表） */
static int gbk_to_utf8(uint8_t c1, uint8_t c2, char *out)
{
    if (c1 < 0x80) {
        out[0] = c1;
        return 1;
    }
    if (c1 >= 0x81 && c1 <= 0xFE && c2 >= 0x40 && c2 <= 0xFE && c2 != 0x7F) {
        int row = c1 - 0x81;
        int col = c2 - 0x40 - (c2 > 0x7F ? 1 : 0);
        uint32_t cp = gbk_to_unicode[row * 190 + col];
        if (cp == 0) {
            out[0] = '?';
            return 1;
        }
        if (cp < 0x80) {
            out[0] = cp;
            return 1;
        } else if (cp < 0x800) {
            out[0] = 0xC0 | (cp >> 6);
            out[1] = 0x80 | (cp & 0x3F);
            return 2;
        } else {
            out[0] = 0xE0 | (cp >> 12);
            out[1] = 0x80 | ((cp >> 6) & 0x3F);
            out[2] = 0x80 | (cp & 0x3F);
            return 3;
        }
    }
    /* 非法序列：替换为 ? */
    out[0] = '?';
    return 1;
}

/* TXT 解析：检测编码 → 转换为 UTF-8 */
static esp_err_t parse_txt(const char *file_path, char **text_out, uint32_t *len_out)
{
    FILE *f = fopen(file_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "打开 TXT 失败: %s", file_path);
        return ESP_ERR_NOT_FOUND;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > MAX_TXT_SIZE) {
        ESP_LOGE(TAG, "TXT 大小异常: %ld", sz);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *raw = smart_malloc(sz);
    if (!raw) { fclose(f); return ESP_ERR_NO_MEM; }
    fread(raw, 1, sz, f);
    fclose(f);

    /* 跳过 UTF-8 BOM */
    size_t start = 0;
    if (sz >= 3 && memcmp(raw, UTF8_BOM, 3) == 0) start = 3;

    /* 启发式编码检测：扫描首 1KB */
    bool is_gbk = !looks_like_utf8(raw + start, (size_t)sz - start < 1024 ? (size_t)sz - start : 1024);
    ESP_LOGI(TAG, "TXT 编码检测: %s", is_gbk ? "GBK" : "UTF-8");

    char *out;
    if (is_gbk) {
        /* GBK → UTF-8：最坏 1.5x 膨胀 */
        out = smart_calloc(sz * 3 / 2 + 4, 1);
        if (!out) { free(raw); return ESP_ERR_NO_MEM; }
        size_t op = 0;
        for (size_t i = start; i < (size_t)sz; ) {
            uint8_t b1 = raw[i];
            if (b1 < 0x80) {
                out[op++] = b1;
                i++;
            } else if (i + 1 < (size_t)sz) {
                int n = gbk_to_utf8(b1, raw[i+1], out + op);
                op += n;
                i += 2;
            } else {
                out[op++] = '?';
                i++;
            }
        }
        out[op] = '\0';
        *len_out = op;
    } else {
        /* 已是 UTF-8：去掉 BOM 后拷贝 */
        size_t text_len = sz - start;
        out = smart_malloc(text_len + 1);
        if (!out) { free(raw); return ESP_ERR_NO_MEM; }
        memcpy(out, raw + start, text_len);
        out[text_len] = '\0';
        *len_out = text_len;
    }

    free(raw);
    *text_out = out;
    ESP_LOGI(TAG, "TXT 解析完成: %u 字节", (unsigned)*len_out);
    return ESP_OK;
}

/* ZIP 解析（支持 stored + deflate） */
typedef struct {
    char     name[128];
    uint32_t offset;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t comp_method;
} ZipEntry;

/* 扫描中央目录查找条目 */
static int find_zip_entry(const uint8_t *data, size_t size, const char *name, ZipEntry *out)
{
    /* 中央目录签名 0x02014b50 */
    const uint8_t CD_SIG[] = {0x50, 0x4B, 0x01, 0x02};
    for (size_t i = 0; i + 46 <= size; i++) {
        if (memcmp(data + i, CD_SIG, 4) != 0) continue;
        uint16_t comp_method  = data[i+10] | (data[i+11] << 8);
        uint32_t comp_size    = data[i+20] | (data[i+21] << 8) | (data[i+22] << 16) | (data[i+23] << 24);
        uint32_t uncomp_size  = data[i+24] | (data[i+25] << 8) | (data[i+26] << 16) | (data[i+27] << 24);
        uint32_t fname_len    = data[i+28] | (data[i+29] << 8);
        uint32_t extra_len    = data[i+30] | (data[i+31] << 8);
        uint32_t comment_len  = data[i+32] | (data[i+33] << 8);
        uint32_t local_offset = data[i+42] | (data[i+43] << 8) | (data[i+44] << 16) | (data[i+45] << 24);
        if (i + 46 + fname_len > size) continue;
        if (fname_len >= sizeof(out->name)) continue;
        if (memcmp(data + i + 46, name, fname_len) == 0 && name[fname_len] == '\0') {
            strncpy(out->name, name, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';
            out->comp_size = comp_size;
            out->uncomp_size = uncomp_size;
            out->comp_method = comp_method;
            out->offset = local_offset;
            return 0;
        }
        i += 46 + fname_len + extra_len + comment_len;
    }
    return -1;
}

/* 读取 ZIP 条目（支持 stored + deflate） */
static esp_err_t read_zip_entry(const uint8_t *data, size_t size, const ZipEntry *e, char **out, uint32_t *out_len)
{
    /* 本地文件头：30 + fname + extra */
    if (e->offset + 30 > size) return ESP_ERR_INVALID_SIZE;
    uint16_t fname_len = data[e->offset + 26] | (data[e->offset + 27] << 8);
    uint16_t extra_len = data[e->offset + 28] | (data[e->offset + 29] << 8);
    uint32_t data_off  = e->offset + 30 + fname_len + extra_len;
    if (data_off + e->comp_size > size) return ESP_ERR_INVALID_SIZE;

    if (e->comp_method == 0) {
        /* stored: 直接拷贝 */
        char *buf = smart_malloc(e->comp_size + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        memcpy(buf, data + data_off, e->comp_size);
        buf[e->comp_size] = '\0';
        *out = buf;
        *out_len = e->comp_size;
    } else if (e->comp_method == 8) {
        /* deflate: 使用 ROM miniz 解压 */
        size_t out_buf_len = e->uncomp_size > 0 ? e->uncomp_size + 1 : e->comp_size * 4 + 1;
        char *buf = smart_malloc(out_buf_len);
        if (!buf) return ESP_ERR_NO_MEM;

        size_t result = tinfl_decompress_mem_to_mem(buf, out_buf_len - 1,
            data + data_off, e->comp_size,
            TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        if (result == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
            ESP_LOGE(TAG, "ZIP deflate 解压失败: %s", e->name);
            free(buf);
            return ESP_FAIL;
        }
        buf[result] = '\0';
        *out = buf;
        *out_len = (uint32_t)result;
        ESP_LOGD(TAG, "ZIP deflate 解压: %s (%u → %u bytes)", e->name, (unsigned)e->comp_size, (unsigned)result);
    } else {
        ESP_LOGW(TAG, "ZIP 条目 %s 压缩方法 %u 不支持", e->name, e->comp_method);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

/* HTML 标签剥离：状态机 */
static void strip_html(const char *in, uint32_t in_len, char *out, uint32_t *out_len)
{
    uint32_t op = 0;
    bool in_tag = false;
    char tag_name[32] = {0};

    for (uint32_t i = 0; i < in_len; i++) {
        char c = in[i];
        if (in_tag) {
            if (c == '>') {
                in_tag = false;
                /* 段落/换行标签：输出 \n */
                if (strcmp(tag_name, "p") == 0 || strcmp(tag_name, "P") == 0 ||
                    strcmp(tag_name, "br") == 0 || strcmp(tag_name, "BR") == 0 ||
                    strcmp(tag_name, "div") == 0 || strcmp(tag_name, "DIV") == 0) {
                    out[op++] = '\n';
                }
                tag_name[0] = '\0';
            } else if (c != '<' && (strlen(tag_name) < sizeof(tag_name) - 1)) {
                size_t l = strlen(tag_name);
                tag_name[l] = c;
                tag_name[l+1] = '\0';
            }
            continue;
        }
        if (c == '<') {
            in_tag = true;
            tag_name[0] = '\0';
            continue;
        }
        if (c == '&') {
            /* 简单 HTML 实体 */
            if (i + 4 < in_len && memcmp(in + i, "&amp;", 5) == 0) { out[op++] = '&'; i += 4; continue; }
            if (i + 3 < in_len && memcmp(in + i, "&lt;", 4)  == 0) { out[op++] = '<'; i += 3; continue; }
            if (i + 3 < in_len && memcmp(in + i, "&gt;", 4)  == 0) { out[op++] = '>'; i += 3; continue; }
            if (i + 5 < in_len && memcmp(in + i, "&quot;", 6) == 0) { out[op++] = '"'; i += 5; continue; }
            if (i + 5 < in_len && memcmp(in + i, "&apos;", 6) == 0) { out[op++] = '\''; i += 5; continue; }
            out[op++] = '&';
            continue;
        }
        out[op++] = c;
    }
    out[op] = '\0';
    *out_len = op;
}

/* EPUB 解析：META-INF/container.xml → OPF → spine items */
static esp_err_t parse_epub(const char *file_path, char **text_out, uint32_t *len_out)
{
    FILE *f = fopen(file_path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > MAX_EPUB_SIZE) { fclose(f); return ESP_ERR_INVALID_SIZE; }
    uint8_t *data = smart_malloc(sz);
    if (!data) { fclose(f); return ESP_ERR_NO_MEM; }
    fread(data, 1, sz, f);
    fclose(f);

    /* 1. 读 META-INF/container.xml 找 OPF 路径 */
    ZipEntry e;
    char *container_xml = NULL;
    uint32_t container_len = 0;
    if (find_zip_entry(data, sz, "META-INF/container.xml", &e) == 0) {
        read_zip_entry(data, sz, &e, &container_xml, &container_len);
    }
    if (!container_xml) {
        ESP_LOGE(TAG, "EPUB 缺少 container.xml");
        free(data);
        return ESP_ERR_NOT_FOUND;
    }
    cJSON *root = cJSON_Parse(container_xml);
    free(container_xml);
    if (!root) {
        free(data);
        return ESP_FAIL;
    }
    cJSON *rootfile = cJSON_GetObjectItem(root, "rootfile");
    cJSON *path = rootfile ? cJSON_GetObjectItem(rootfile, "full-path") : NULL;
    if (!path || !cJSON_IsString(path)) {
        cJSON_Delete(root);
        free(data);
        return ESP_FAIL;
    }
    char opf_path[128];
    strncpy(opf_path, path->valuestring, sizeof(opf_path) - 1);
    opf_path[sizeof(opf_path) - 1] = '\0';
    cJSON_Delete(root);

    /* 2. 读 OPF 文件，提取 <spine><itemref> 顺序 */
    char *opf_xml = NULL;
    uint32_t opf_len = 0;
    if (find_zip_entry(data, sz, opf_path, &e) != 0 ||
        read_zip_entry(data, sz, &e, &opf_xml, &opf_len) != ESP_OK) {
        ESP_LOGE(TAG, "EPUB OPF 读取失败: %s", opf_path);
        free(data);
        return ESP_FAIL;
    }
    cJSON *opf = cJSON_Parse(opf_xml);
    free(opf_xml);
    if (!opf) { free(data); return ESP_FAIL; }

    /* 3. 拼接所有 spine 文本（简化：按 manifest 顺序扫，遇到 XHTML 读出） */
    cJSON *manifest = cJSON_GetObjectItem(opf, "manifest");
    cJSON *item = manifest ? cJSON_GetObjectItem(manifest, "item") : NULL;
    /* 计算 OPF 所在目录前缀 */
    char opf_dir[128] = {0};
    const char *slash = strrchr(opf_path, '/');
    if (slash) {
        size_t l = slash - opf_path + 1;
        memcpy(opf_dir, opf_path, l);
        opf_dir[l] = '\0';
    }

    /* 输出缓冲：初始 1MB，按需扩展 */
    uint32_t cap = 1 * 1024 * 1024;
    char *out = smart_malloc(cap);
    if (!out) { cJSON_Delete(opf); free(data); return ESP_ERR_NO_MEM; }
    uint32_t op = 0;

    if (cJSON_IsArray(item)) {
        cJSON *it;
        cJSON_ArrayForEach(it, item) {
            cJSON *href = cJSON_GetObjectItem(it, "href");
            cJSON *mt   = cJSON_GetObjectItem(it, "media-type");
            if (!cJSON_IsString(href) || !cJSON_IsString(mt)) continue;
            if (strstr(mt->valuestring, "xhtml") == NULL) continue;
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s%s", opf_dir, href->valuestring);
            ZipEntry ze;
            if (find_zip_entry(data, sz, full_path, &ze) != 0) continue;
            char *xhtml = NULL;
            uint32_t xhtml_len = 0;
            if (read_zip_entry(data, sz, &ze, &xhtml, &xhtml_len) != ESP_OK) continue;
            /* 扩展缓冲区 */
            if (op + xhtml_len + 16 > cap) {
                cap *= 2;
                char *np = smart_realloc(out, cap);
                if (!np) { free(xhtml); continue; }
                out = np;
            }
            strip_html(xhtml, xhtml_len, out + op, &xhtml_len);
            op += xhtml_len;
            out[op++] = '\n';
            free(xhtml);
        }
    }

    cJSON_Delete(opf);
    free(data);
    out[op] = '\0';
    *text_out = out;
    *len_out = op;
    ESP_LOGI(TAG, "EPUB 解析完成: %u 字节", (unsigned)op);
    return ESP_OK;
}

esp_err_t book_load_text(const char *file_path, char **text_out, uint32_t *len_out)
{
    if (!file_path || !text_out || !len_out) return ESP_ERR_INVALID_ARG;
    BookFormat fmt = book_detect_format(file_path);
    if (fmt == BOOK_TXT)  return parse_txt(file_path, text_out, len_out);
    if (fmt == BOOK_EPUB) return parse_epub(file_path, text_out, len_out);
    return ESP_ERR_NOT_SUPPORTED;
}

/* MD5 简化（取前 16 字符 hex 即可）— 实际用摘要算法 */
static void make_book_id(const char *path, char *id_out)
{
    /* FNV-1a 32-bit → 8 hex 字符 */
    uint32_t h = 2166136261u;
    for (const char *p = path; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    snprintf(id_out, 17, "%08lx%08lx", (unsigned long)h, (unsigned long)(h ^ 0xDEADBEEFu));
}

esp_err_t book_extract_metadata(const char *file_path, BookMeta *meta)
{
    if (!file_path || !meta) return ESP_ERR_INVALID_ARG;
    memset(meta, 0, sizeof(*meta));
    make_book_id(file_path, meta->id);
    strncpy(meta->filePath, file_path, sizeof(meta->filePath) - 1);

    /* 书名 = 文件名去扩展名 */
    const char *base = strrchr(file_path, '/');
    base = base ? base + 1 : file_path;
    const char *dot = strrchr(base, '.');
    size_t name_len = dot ? (size_t)(dot - base) : strlen(base);
    if (name_len >= sizeof(meta->title)) name_len = sizeof(meta->title) - 1;
    memcpy(meta->title, base, name_len);
    meta->title[name_len] = '\0';

    /* TODO: EPUB 解析 OPF 提取 dc:title/dc:creator */
    return ESP_OK;
}

void book_free_text(char *text)
{
    if (text) free(text);
}

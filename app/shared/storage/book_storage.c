/**
 * @file book_storage.c
 * @brief JSON 持久化存储模块实现
 *
 * 管理阅读进度和书签数据，使用简单 JSON 格式存储。
 * 使用 #ifdef PC_SIMULATION 包裹实现。
 */
#include "book_storage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#ifdef PC_SIMULATION

/* 数据结构定义 */
typedef struct {
    char name[256];
    int page;
    long timestamp;
} ProgressEntry;

typedef struct {
    char name[256];
    int pages[BOOKMARK_MAX_PER_BOOK];
    long timestamps[BOOKMARK_MAX_PER_BOOK];
    int count;
} BookmarkEntry;

/* 全局数据 */
#define MAX_BOOKS 100
#define DATA_DIR "data"
#define DATA_FILE "data/reading_data.json"

static ProgressEntry g_progress[MAX_BOOKS];
static int g_progress_count = 0;

static BookmarkEntry g_bookmarks[MAX_BOOKS];
static int g_bookmarks_count = 0;

/* 辅助函数：查找进度条目 */
static ProgressEntry* find_progress(const char *book_name)
{
    for (int i = 0; i < g_progress_count; i++) {
        if (strcmp(g_progress[i].name, book_name) == 0) {
            return &g_progress[i];
        }
    }
    return NULL;
}

/* 辅助函数：查找或创建进度条目 */
static ProgressEntry* get_or_create_progress(const char *book_name)
{
    ProgressEntry *entry = find_progress(book_name);
    if (entry) return entry;

    if (g_progress_count >= MAX_BOOKS) return NULL;

    entry = &g_progress[g_progress_count++];
    strncpy(entry->name, book_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->page = 0;
    entry->timestamp = 0;
    return entry;
}

/* 辅助函数：查找书签条目 */
static BookmarkEntry* find_bookmark(const char *book_name)
{
    for (int i = 0; i < g_bookmarks_count; i++) {
        if (strcmp(g_bookmarks[i].name, book_name) == 0) {
            return &g_bookmarks[i];
        }
    }
    return NULL;
}

/* 辅助函数：查找或创建书签条目 */
static BookmarkEntry* get_or_create_bookmark(const char *book_name)
{
    BookmarkEntry *entry = find_bookmark(book_name);
    if (entry) return entry;

    if (g_bookmarks_count >= MAX_BOOKS) return NULL;

    entry = &g_bookmarks[g_bookmarks_count++];
    strncpy(entry->name, book_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->count = 0;
    return entry;
}

/* 简单 JSON 解析：从文件加载数据 */
static void load_from_file(void)
{
    FILE *f = fopen(DATA_FILE, "r");
    if (!f) return;

    /* 读取整个文件到缓冲区 */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 65536) {
        fclose(f);
        return;
    }

    char *buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        return;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    /* 简单解析：查找 progress 和 bookmarks 部分 */
    char *p = buf;

    /* 解析 progress 部分 */
    char *progress_start = strstr(p, "\"progress\":{");
    if (progress_start) {
        progress_start += 12; /* 跳过 "progress":{ */
        while (*progress_start && g_progress_count < MAX_BOOKS) {
            /* 查找书名 */
            char *name_start = strchr(progress_start, '"');
            if (!name_start || name_start > strchr(progress_start, '}')) break;
            name_start++;

            char *name_end = strchr(name_start, '"');
            if (!name_end) break;

            size_t name_len = name_end - name_start;
            if (name_len >= 256) name_len = 255;

            ProgressEntry *entry = &g_progress[g_progress_count];
            strncpy(entry->name, name_start, name_len);
            entry->name[name_len] = '\0';

            /* 查找 page 和 timestamp */
            char *page_str = strstr(name_end, "\"page\":");
            char *ts_str = strstr(name_end, "\"timestamp\":");

            if (page_str) {
                entry->page = atoi(page_str + 7);
            }
            if (ts_str) {
                entry->timestamp = atol(ts_str + 12);
            }

            g_progress_count++;
            progress_start = name_end + 1;
        }
    }

    /* 解析 bookmarks 部分 */
    char *bookmarks_start = strstr(p, "\"bookmarks\":{");
    if (bookmarks_start) {
        bookmarks_start += 13; /* 跳过 "bookmarks":{ */
        while (*bookmarks_start && g_bookmarks_count < MAX_BOOKS) {
            /* 查找书名 */
            char *name_start = strchr(bookmarks_start, '"');
            if (!name_start) break;
            name_start++;

            char *name_end = strchr(name_start, '"');
            if (!name_end) break;

            size_t name_len = name_end - name_start;
            if (name_len >= 256) name_len = 255;

            BookmarkEntry *entry = &g_bookmarks[g_bookmarks_count];
            strncpy(entry->name, name_start, name_len);
            entry->name[name_len] = '\0';
            entry->count = 0;

            /* 查找数组开始 */
            char *array_start = strchr(name_end, '[');
            if (array_start) {
                char *pos = array_start + 1;
                while (*pos && *pos != ']' && entry->count < BOOKMARK_MAX_PER_BOOK) {
                    /* 查找 page */
                    char *page_str = strstr(pos, "\"page\":");
                    if (!page_str || page_str > strchr(pos, ']')) break;

                    entry->pages[entry->count] = atoi(page_str + 7);

                    /* 查找 timestamp */
                    char *ts_str = strstr(page_str, "\"timestamp\":");
                    if (ts_str) {
                        entry->timestamps[entry->count] = atol(ts_str + 12);
                    } else {
                        entry->timestamps[entry->count] = 0;
                    }

                    entry->count++;
                    pos = page_str + 7;
                }
            }

            g_bookmarks_count++;
            bookmarks_start = name_end + 1;
        }
    }

    free(buf);
}

/* 辅助函数：转义 JSON 字符串中的特殊字符 */
static void write_escaped_string(FILE *f, const char *str)
{
    fprintf(f, "\"");
    while (*str) {
        if (*str == '"' || *str == '\\') {
            fprintf(f, "\\%c", *str);
        } else if (*str == '\n') {
            fprintf(f, "\\n");
        } else {
            fprintf(f, "%c", *str);
        }
        str++;
    }
    fprintf(f, "\"");
}

/* 保存数据到文件 */
static void save_to_file(void)
{
    FILE *f = fopen(DATA_FILE, "w");
    if (!f) return;

    fprintf(f, "{\n");

    /* 写入 progress */
    fprintf(f, "  \"progress\":{");
    for (int i = 0; i < g_progress_count; i++) {
        if (i > 0) fprintf(f, ",");
        fprintf(f, "\n    ");
        write_escaped_string(f, g_progress[i].name);
        fprintf(f, ":{\"page\":%d,\"timestamp\":%ld}", g_progress[i].page, g_progress[i].timestamp);
    }
    if (g_progress_count > 0) fprintf(f, "\n  ");
    fprintf(f, "},\n");

    /* 写入 bookmarks */
    fprintf(f, "  \"bookmarks\":{");
    for (int i = 0; i < g_bookmarks_count; i++) {
        if (i > 0) fprintf(f, ",");
        fprintf(f, "\n    ");
        write_escaped_string(f, g_bookmarks[i].name);
        fprintf(f, ":[");
        for (int j = 0; j < g_bookmarks[i].count; j++) {
            if (j > 0) fprintf(f, ",");
            fprintf(f, "\n      {\"page\":%d,\"timestamp\":%ld}",
                    g_bookmarks[i].pages[j], g_bookmarks[i].timestamps[j]);
        }
        if (g_bookmarks[i].count > 0) fprintf(f, "\n    ");
        fprintf(f, "]");
    }
    if (g_bookmarks_count > 0) fprintf(f, "\n  ");
    fprintf(f, "}\n");

    fprintf(f, "}\n");
    fclose(f);
}

/* 公共接口实现 */

void book_storage_init(void)
{
    g_progress_count = 0;
    g_bookmarks_count = 0;

    /* 创建 data 目录 */
    mkdir(DATA_DIR, 0755);

    /* 加载已有数据 */
    load_from_file();
}

void book_storage_save_progress(const char *book_name, int page)
{
    if (!book_name) return;

    ProgressEntry *entry = get_or_create_progress(book_name);
    if (!entry) return;

    entry->page = page;
    entry->timestamp = (long)time(NULL);
}

int book_storage_load_progress(const char *book_name)
{
    if (!book_name) return -1;

    ProgressEntry *entry = find_progress(book_name);
    if (!entry) return -1;

    return entry->page;
}

void book_storage_add_bookmark(const char *book_name, int page)
{
    if (!book_name) return;

    BookmarkEntry *entry = get_or_create_bookmark(book_name);
    if (!entry) return;

    /* 检查是否已存在 */
    for (int i = 0; i < entry->count; i++) {
        if (entry->pages[i] == page) return;
    }

    /* 如果满了，删除最早的（左移） */
    if (entry->count >= BOOKMARK_MAX_PER_BOOK) {
        for (int i = 0; i < BOOKMARK_MAX_PER_BOOK - 1; i++) {
            entry->pages[i] = entry->pages[i + 1];
            entry->timestamps[i] = entry->timestamps[i + 1];
        }
        entry->count = BOOKMARK_MAX_PER_BOOK - 1;
    }

    /* 添加新书签 */
    entry->pages[entry->count] = page;
    entry->timestamps[entry->count] = (long)time(NULL);
    entry->count++;
}

int book_storage_get_bookmarks(const char *book_name, int *pages, int max_count)
{
    if (!book_name || !pages || max_count <= 0) return 0;

    BookmarkEntry *entry = find_bookmark(book_name);
    if (!entry) return 0;

    int count = entry->count;
    if (count > max_count) count = max_count;

    for (int i = 0; i < count; i++) {
        pages[i] = entry->pages[i];
    }

    return count;
}

int book_storage_has_bookmark(const char *book_name, int page)
{
    if (!book_name) return 0;

    BookmarkEntry *entry = find_bookmark(book_name);
    if (!entry) return 0;

    for (int i = 0; i < entry->count; i++) {
        if (entry->pages[i] == page) return 1;
    }

    return 0;
}

void book_storage_save(void)
{
    save_to_file();
}

#endif /* PC_SIMULATION */

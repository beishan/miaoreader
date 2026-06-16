/**
 * @file mock_books.c
 * @brief Mock 书籍模块 - 本地目录扫描
 *
 * 扫描 shared/books/ 目录，使用 book_parser 提取元数据，
 * 集成 book_storage 加载阅读进度。
 */
#ifdef PC_SIMULATION

#include "mock_books.h"
#include "../engine/book_parser.h"
#include "../storage/book_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_BOOKS       32
#define BOOKS_DIR       "shared/books"
#define PATH_BUFSIZE    512

/* 书籍内部数据 */
typedef struct {
    char filename[256];         /* 文件名 */
    char path[PATH_BUFSIZE];    /* 完整路径 */
    char title[128];            /* 书名 */
    char author[64];            /* 作者 */
    int total_chars;            /* 总字符数 */
    int current_page;           /* 当前页（从 book_storage 恢复） */
    int estimated_pages;        /* 估算总页数 */
    bool is_current_reading;    /* 是否正在阅读 */
} BookEntry;

static BookEntry s_books[MAX_BOOKS];
static int s_book_count = 0;

/* 统计模拟数据 */
static uint32_t s_today_minutes = 38;
static uint32_t s_total_hours = 127;

/* 估算页数：按每页约 500 字符估算 */
static int estimate_pages(int total_chars)
{
    if (total_chars <= 0) return 1;
    return (total_chars + 499) / 500;
}

void mock_books_init(void)
{
    s_book_count = 0;

    DIR *dir = opendir(BOOKS_DIR);
    if (!dir) {
        printf("[Mock Books] 目录 %s 不存在或无法打开\n", BOOKS_DIR);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_book_count < MAX_BOOKS) {
        /* 跳过隐藏文件和非书籍文件 */
        if (entry->d_name[0] == '.') continue;

        /* 检查扩展名 */
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext) continue;
        if (strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".epub") != 0) continue;

        /* 检测格式 */
        char full_path[PATH_BUFSIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", BOOKS_DIR, entry->d_name);
        BookFormat fmt = book_parser_detect(full_path);
        if (fmt == BOOK_FMT_UNKNOWN) continue;

        /* 提取元数据 */
        BookInfo info;
        memset(&info, 0, sizeof(info));
        if (book_parser_extract_info(full_path, &info) != 0) {
            printf("[Mock Books] 无法解析: %s\n", entry->d_name);
            continue;
        }

        /* 填充 BookEntry */
        BookEntry *book = &s_books[s_book_count];
        memset(book, 0, sizeof(BookEntry));
        strncpy(book->filename, entry->d_name, sizeof(book->filename) - 1);
        strncpy(book->path, full_path, sizeof(book->path) - 1);
        strncpy(book->title, info.title, sizeof(book->title) - 1);
        strncpy(book->author, info.author, sizeof(book->author) - 1);
        book->total_chars = info.total_chars;
        book->estimated_pages = estimate_pages(info.total_chars);

        /* 从 book_storage 恢复阅读进度 */
        int saved_page = book_storage_load_progress(info.title);
        if (saved_page >= 0) {
            book->current_page = saved_page;
            book->is_current_reading = true;
        } else {
            book->current_page = 0;
            book->is_current_reading = false;
        }

        printf("[Mock Books] 加载: %s (作者: %s, %d字, %d页)\n",
               book->title, book->author, book->total_chars, book->estimated_pages);

        s_book_count++;
    }

    closedir(dir);

    /* 如果没有正在阅读的书，标记第一本为当前阅读 */
    bool has_reading = false;
    for (int i = 0; i < s_book_count; i++) {
        if (s_books[i].is_current_reading) {
            has_reading = true;
            break;
        }
    }
    if (!has_reading && s_book_count > 0) {
        s_books[0].is_current_reading = true;
    }

    printf("[Mock Books] 初始化完成，共 %d 本书\n", s_book_count);
}

int mock_books_get_count(void)
{
    return s_book_count;
}

int mock_books_get_by_index(int index, MockBookMeta *book)
{
    if (index < 0 || index >= s_book_count || !book) {
        return -1;
    }

    BookEntry *entry = &s_books[index];
    memset(book, 0, sizeof(MockBookMeta));
    snprintf(book->id, sizeof(book->id), "book_%03d", index);
    strncpy(book->title, entry->title, sizeof(book->title) - 1);
    strncpy(book->author, entry->author, sizeof(book->author) - 1);
    strncpy(book->filePath, entry->path, sizeof(book->filePath) - 1);
    book->totalPages = entry->estimated_pages;
    book->currentPage = entry->current_page;
    book->readingSeconds = 0;
    book->isCurrentReading = entry->is_current_reading;

    return 0;
}

int mock_books_get_current_reading(MockBookMeta *book)
{
    for (int i = 0; i < s_book_count; i++) {
        if (s_books[i].is_current_reading) {
            return mock_books_get_by_index(i, book);
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
        /* 截断过长的书名 */
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

int mock_books_get_path(int index, char *path, int size)
{
    if (index < 0 || index >= s_book_count || !path || size <= 0) {
        return -1;
    }
    strncpy(path, s_books[index].path, size - 1);
    path[size - 1] = '\0';
    return 0;
}

char *mock_books_load_text(int index)
{
    if (index < 0 || index >= s_book_count) {
        return NULL;
    }
    return book_parser_load_text(s_books[index].path);
}

const char *mock_books_get_filename(int index)
{
    if (index < 0 || index >= s_book_count) {
        return NULL;
    }
    return s_books[index].filename;
}

#endif /* PC_SIMULATION */

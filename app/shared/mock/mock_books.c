/**
 * @file mock_books.c
 * @brief Mock 书籍模块 - 模拟书库数据
 */
#ifdef PC_SIMULATION

#include "mock_books.h"
#include <stdio.h>
#include <string.h>

/* Mock 书籍数据 */
static MockBookMeta s_books[] = {
    {
        .id = "book_001",
        .title = "三体",
        .author = "刘慈欣",
        .filePath = "/books/三体.txt",
        .totalPages = 312,
        .currentPage = 45,
        .readingSeconds = 3600,
        .isCurrentReading = true,
    },
    {
        .id = "book_002",
        .title = "百年孤独",
        .author = "马尔克斯",
        .filePath = "/books/百年孤独.epub",
        .totalPages = 420,
        .currentPage = 0,
        .readingSeconds = 0,
        .isCurrentReading = false,
    },
    {
        .id = "book_003",
        .title = "活着",
        .author = "余华",
        .filePath = "/books/活着.txt",
        .totalPages = 180,
        .currentPage = 180,
        .readingSeconds = 7200,
        .isCurrentReading = false,
    },
    {
        .id = "book_004",
        .title = "1984",
        .author = "乔治·奥威尔",
        .filePath = "/books/1984.txt",
        .totalPages = 250,
        .currentPage = 120,
        .readingSeconds = 5400,
        .isCurrentReading = false,
    },
    {
        .id = "book_005",
        .title = "红楼梦",
        .author = "曹雪芹",
        .filePath = "/books/红楼梦.txt",
        .totalPages = 980,
        .currentPage = 560,
        .readingSeconds = 18000,
        .isCurrentReading = false,
    },
    {
        .id = "book_006",
        .title = "西游记",
        .author = "吴承恩",
        .filePath = "/books/西游记.txt",
        .totalPages = 860,
        .currentPage = 230,
        .readingSeconds = 9000,
        .isCurrentReading = false,
    },
    {
        .id = "book_007",
        .title = "水浒传",
        .author = "施耐庵",
        .filePath = "/books/水浒传.txt",
        .totalPages = 750,
        .currentPage = 0,
        .readingSeconds = 0,
        .isCurrentReading = false,
    },
    {
        .id = "book_008",
        .title = "三国演义",
        .author = "罗贯中",
        .filePath = "/books/三国演义.txt",
        .totalPages = 680,
        .currentPage = 340,
        .readingSeconds = 12000,
        .isCurrentReading = false,
    },
    {
        .id = "book_009",
        .title = "围城",
        .author = "钱钟书",
        .filePath = "/books/围城.txt",
        .totalPages = 320,
        .currentPage = 80,
        .readingSeconds = 4500,
        .isCurrentReading = false,
    },
    {
        .id = "book_010",
        .title = "平凡的世界",
        .author = "路遥",
        .filePath = "/books/平凡的世界.txt",
        .totalPages = 1200,
        .currentPage = 450,
        .readingSeconds = 15000,
        .isCurrentReading = false,
    },
    {
        .id = "book_011",
        .title = "白鹿原",
        .author = "陈忠实",
        .filePath = "/books/白鹿原.txt",
        .totalPages = 560,
        .currentPage = 0,
        .readingSeconds = 0,
        .isCurrentReading = false,
    },
    {
        .id = "book_012",
        .title = "骆驼祥子",
        .author = "老舍",
        .filePath = "/books/骆驼祥子.txt",
        .totalPages = 200,
        .currentPage = 200,
        .readingSeconds = 6000,
        .isCurrentReading = false,
    },
};

#define BOOK_COUNT (sizeof(s_books) / sizeof(s_books[0]))

/* 模拟今日阅读时间 (分钟) */
static uint32_t s_today_minutes = 38;

/* 模拟累计阅读时间 (小时) */
static uint32_t s_total_hours = 127;

void mock_books_init(void)
{
    printf("[Mock Books] 初始化，共 %d 本书\n", (int)BOOK_COUNT);
}

int mock_books_get_count(void)
{
    return (int)BOOK_COUNT;
}

int mock_books_get_by_index(int index, MockBookMeta *book)
{
    if (index < 0 || index >= (int)BOOK_COUNT || !book) {
        return -1;
    }
    *book = s_books[index];
    return 0;
}

int mock_books_get_current_reading(MockBookMeta *book)
{
    for (int i = 0; i < (int)BOOK_COUNT; i++) {
        if (s_books[i].isCurrentReading) {
            if (book) {
                *book = s_books[i];
            }
            return 0;
        }
    }
    return -1;
}

void mock_books_get_count_str(char *buf, int size)
{
    snprintf(buf, size, "藏书: %d 册", (int)BOOK_COUNT);
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

#endif /* PC_SIMULATION */

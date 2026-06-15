/**
 * @file page_bookshelf.c
 * @brief 书架页：3×2 网格 + 默认封面 + 翻页 + 选中导航
 */
#include "page_bookshelf.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "engine/types.h"
#include "engine/config.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

static const char *TAG = "page_bookshelf";

#define GRID_COLS  3
#define GRID_ROWS  2
#define CELL_W     120
#define CELL_H     130
#define GRID_X_OFS 20
#define GRID_Y_OFS 30
#define MAX_BOOKS  64

static BookMeta s_books[MAX_BOOKS];
static int s_book_count = 0;
static int s_current_page = 0;
static int s_selected = 0;
static int s_total_pages = 1;

/* 扫描 SD:/books/ 目录，构造 BookMeta 数组 */
static void load_books(void)
{
    s_book_count = 0;
    DIR *d = opendir("/sd/books");
    if (!d) {
        ESP_LOGW(TAG, "/sd/books 目录不存在，使用占位数据");
        /* 占位 3 本测试书 */
        for (int i = 0; i < 3 && s_book_count < MAX_BOOKS; i++) {
            BookMeta *m = &s_books[s_book_count++];
            memset(m, 0, sizeof(*m));
            snprintf(m->title, sizeof(m->title), "占位书 %d", i + 1);
            snprintf(m->filePath, sizeof(m->filePath), "/sd/books/placeholder_%d.txt", i + 1);
            snprintf(m->id, sizeof(m->id), "placeholder%d", i + 1);
        }
    } else {
        struct dirent *e;
        while ((e = readdir(d)) != NULL && s_book_count < MAX_BOOKS) {
            if (e->d_name[0] == '.') continue;
            const char *ext = strrchr(e->d_name, '.');
            if (!ext) continue;
            if (strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".epub") != 0) continue;
            BookMeta *m = &s_books[s_book_count];
            memset(m, 0, sizeof(*m));
            strncpy(m->title, e->d_name, sizeof(m->title) - 1);
            /* 去扩展名 */
            char *dot = strrchr(m->title, '.');
            if (dot) *dot = '\0';
            snprintf(m->filePath, sizeof(m->filePath), "/sd/books/%.200s", e->d_name);
            snprintf(m->id, sizeof(m->id), "book%d", s_book_count);
            s_book_count++;
        }
        closedir(d);
    }
    s_total_pages = (s_book_count + GRID_COLS * GRID_ROWS - 1) / (GRID_COLS * GRID_ROWS);
    if (s_total_pages < 1) s_total_pages = 1;
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入书架");
    load_books();
    s_current_page = 0;
    s_selected = 0;
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    int per_page = GRID_COLS * GRID_ROWS;
    int start = s_current_page * per_page;

    for (int i = 0; i < per_page; i++) {
        int idx = start + i;
        if (idx >= s_book_count) break;
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int x = GRID_X_OFS + col * CELL_W;
        int y = GRID_Y_OFS + row * CELL_H;

        /* 封面：白底黑框 + "书" 图标 */
        epd_draw_rect(x, y, 100, 100, EPD_COLOR_BLACK);
        widget_draw_icon(x + 35, y + 30, ICON_BOOK, EPD_COLOR_BLACK);

        /* 选中框（红色） */
        if (idx == s_selected) {
            epd_draw_rect(x - 2, y - 2, 104, 104, EPD_COLOR_RED);
        }

        /* 书名（截断：>10 字符用 ...） */
        char title[20];
        strncpy(title, s_books[idx].title, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';
        if (strlen(s_books[idx].title) >= sizeof(title) - 1) {
            title[sizeof(title) - 4] = '.';
            title[sizeof(title) - 3] = '.';
            title[sizeof(title) - 2] = '.';
            title[sizeof(title) - 1] = '\0';
        }
        widget_draw_text(x, y + 105, title, EPD_COLOR_BLACK);
    }

    /* 翻页指示 */
    char info[32];
    snprintf(info, sizeof(info), "%d/%d  共%d本", s_current_page + 1, s_total_pages, s_book_count);
    widget_draw_text(20, EPD_HEIGHT - 14, info, EPD_COLOR_BLACK);

    epd_display_full();
}

static void on_key(KeyId key, KeyEvent event)
{
    int per_page = GRID_COLS * GRID_ROWS;

    if (key == KEY_PREV) {
        if (event == KEY_EVT_SHORT) {
            if (s_selected > 0) {
                s_selected--;
                /* 跨页时调整 current_page */
                if (s_selected < s_current_page * per_page) {
                    s_current_page = s_selected / per_page;
                }
            }
        } else if (event == KEY_EVT_LONG) {
            if (s_current_page > 0) {
                s_current_page--;
                s_selected = s_current_page * per_page;
            }
        }
        on_render();
    } else if (key == KEY_NEXT) {
        if (event == KEY_EVT_SHORT) {
            if (s_selected < s_book_count - 1) {
                s_selected++;
                if (s_selected >= (s_current_page + 1) * per_page) {
                    s_current_page = s_selected / per_page;
                }
            }
        } else if (event == KEY_EVT_LONG) {
            if (s_current_page < s_total_pages - 1) {
                s_current_page++;
                s_selected = s_current_page * per_page;
            }
        }
        on_render();
    } else if (key == KEY_HOME && event == KEY_EVT_SHORT) {
        if (s_book_count > 0) {
            page_mgr_switch(PAGE_READER);
        }
    }
}

const PageVtbl page_bookshelf_vtbl = {
    .id = PAGE_BOOKSHELF,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

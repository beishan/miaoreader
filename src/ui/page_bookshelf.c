/**
 * @file page_bookshelf.c
 * @brief 书架页：3×2 网格 + 封面 + 翻页 + 选中导航（匹配仿真布局）
 */
#include "page_bookshelf.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "ui/page_reader.h"
#include "engine/types.h"
#include "engine/config.h"
#include "engine/book_meta.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

static const char *TAG = "page_bookshelf";

#define GRID_COLS  3
#define GRID_ROWS  2
#define CELL_W     100
#define COVER_H    65
#define TITLE_H    16
#define CELL_H     (COVER_H + TITLE_H)  /* 81 */
#define GAP_X      20
#define GAP_Y      12
#define FOOTER_H   20
#define MAX_BOOKS  64

static BookMeta s_books[MAX_BOOKS];
static int s_book_count = 0;
static int s_current_page = 0;
static int s_selected = 0;
static int s_total_pages = 1;

/* 从 book_meta 加载书籍列表 */
static void load_books(void)
{
    s_book_count = 0;

    /* 先同步 SD 卡上的书籍 */
    book_meta_sync_from_sd();

    /* 从 book_meta 加载 */
    int count = book_meta_get_count();
    for (int i = 0; i < count && s_book_count < MAX_BOOKS; i++) {
        if (book_meta_get(i, &s_books[s_book_count]) == ESP_OK) {
            s_book_count++;
        }
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
    int end = start + per_page;
    if (end > s_book_count) end = s_book_count;

    /* 计算起始位置（居中） */
    int avail_h = EPD_HEIGHT - 24 - FOOTER_H - 10; /* 状态栏+页码 */
    int grid_w = GRID_COLS * CELL_W + (GRID_COLS - 1) * GAP_X;
    int grid_h = GRID_ROWS * CELL_H + (GRID_ROWS - 1) * GAP_Y;
    int start_x = (EPD_WIDTH - grid_w) / 2;
    int start_y = 24 + (avail_h - grid_h) / 2;

    for (int i = start; i < end; i++) {
        int idx = i - start;
        int col = idx % GRID_COLS;
        int row = idx / GRID_COLS;
        int x = start_x + col * (CELL_W + GAP_X);
        int y = start_y + row * (CELL_H + GAP_Y);
        bool selected = (i == s_selected);
        int scale = selected ? 6 : 0;

        /* 封面（选中时红色放大） */
        EpdColor cover_color = selected ? EPD_COLOR_RED : EPD_COLOR_BLACK;
        epd_fill_rect(x + 20 - scale, y + 5 - scale, 60 + scale * 2, 45 + scale * 2, cover_color);
        epd_fill_rect(x + 22 - scale, y + 7 - scale, 56 + scale * 2, 41 + scale * 2, EPD_COLOR_WHITE);
        /* 书脊线条 */
        epd_fill_rect(x + 25 - scale, y + 7 - scale, 2, 41 + scale * 2, cover_color);

        /* 书名（封面下方，居中） */
        char title[12];
        strncpy(title, s_books[i].title, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';
        int tw = widget_text_width(title);
        int text_x = x + (CELL_W - tw) / 2;
        widget_draw_text(text_x, y + COVER_H + 2, title, selected ? EPD_COLOR_RED : EPD_COLOR_BLACK);
    }

    /* 页码 + 提示 */
    char info[64];
    snprintf(info, sizeof(info), "第 %d/%d 页  共 %d 本  [确认选书]",
             s_current_page + 1, s_total_pages, s_book_count);
    int iw = widget_text_width(info);
    widget_draw_text((EPD_WIDTH - iw) / 2, EPD_HEIGHT - 18, info, EPD_COLOR_BLACK);
}

static void on_key(KeyId key, KeyEvent event)
{
    int per_page = GRID_COLS * GRID_ROWS;

    if (key == KEY_PREV) {
        if (event == KEY_EVT_SHORT) {
            if (s_selected > 0) {
                s_selected--;
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
            /* 将选中书籍路径传递给阅读器 */
            page_reader_set_book(s_books[s_selected].filePath);
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

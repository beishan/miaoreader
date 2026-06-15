/**
 * @file page_reader.c
 * @brief 阅读界面：加载书籍 → 排版分页 → 渲染 + 翻页（局部/全刷交替）
 */
#include "page_reader.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "engine/types.h"
#include "engine/config.h"
#include "engine/typesetter.h"
#include "engine/book_parser.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "page_reader";

#define PARTIAL_CLEAN_INTERVAL 5
#define CONTENT_Y 24    /* 状态栏 20 + 4 间距 */
#define CONTENT_H 256   /* 300 - 20 - 24 */
#define PROGRESS_Y 286

static char    *s_book_text = NULL;
static uint32_t s_text_len = 0;
static PageIndex *s_pages = NULL;
static uint32_t s_page_count = 0;
static uint32_t s_current_page = 0;
static int      s_partial_count = 0;
static char     s_current_path[256] = {0};

static void free_book(void)
{
    if (s_book_text) { book_free_text(s_book_text); s_book_text = NULL; }
    if (s_pages)     { typesetter_free_pages(s_pages); s_pages = NULL; }
    s_page_count = 0;
    s_text_len = 0;
    s_current_page = 0;
}

static esp_err_t load_book(const char *file_path)
{
    free_book();
    strncpy(s_current_path, file_path, sizeof(s_current_path) - 1);

    esp_err_t err = book_load_text(file_path, &s_book_text, &s_text_len);
    if (err != ESP_OK || !s_book_text) {
        ESP_LOGE(TAG, "加载书籍失败: %s (0x%x)", file_path, err);
        return err;
    }

    ReaderConfig rc;
    config_load_reader(&rc);
    TypesetterConfig tc = {
        .fontId = rc.fontId, .fontSize = rc.fontSize,
        .lineSpacing = rc.lineSpacing, .charSpacing = rc.charSpacing,
        .margin = rc.margin,
        .pageWidth = EPD_WIDTH - rc.margin * 2,
        .pageHeight = CONTENT_H - rc.margin * 2,
    };
    err = typesetter_init(&tc);
    if (err == ESP_OK) {
        err = typesetter_paginate(s_book_text, s_text_len, &s_pages, &s_page_count);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "分页失败: 0x%x", err);
        free_book();
        return err;
    }
    ESP_LOGI(TAG, "书籍已加载: %u 字节 → %u 页", (unsigned)s_text_len, (unsigned)s_page_count);
    return ESP_OK;
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入阅读器");
    s_partial_count = 0;
    /* 演示：从占位路径加载（生产环境由 page_bookshelf 注入当前书路径） */
    if (!s_book_text && s_current_path[0] == '\0') {
        const char *demo = "/sd/books/placeholder_1.txt";
        if (load_book(demo) != ESP_OK) {
            /* 失败时填入示例文本 */
            s_book_text = strdup("示例文本\n\n这是一本占位书籍。\n\n按 NEXT 翻页。\n\n按 PREV 返回。\n\n按 HOME 返回书架。");
            s_text_len = strlen(s_book_text);
            s_current_page = 0;
            s_page_count = 1;  /* 占位不分页 */
        }
    }
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    if (s_book_text && s_pages && s_current_page < s_page_count) {
        /* 渲染当前页到 EPD（直接走 epd_set_pixel 接口） */
        typesetter_render_page(s_book_text, &s_pages[s_current_page],
                                NULL, EPD_WIDTH, EPD_HEIGHT, CONTENT_Y);
    } else if (s_book_text) {
        /* 占位模式：用 widget 字体显示示例文本 */
        widget_draw_text(20, 40, s_book_text, EPD_COLOR_BLACK);
    } else {
        widget_draw_text(20, 100, "未加载书籍", EPD_COLOR_RED);
    }

    /* 底部进度条 */
    int bar_x = 120, bar_y = PROGRESS_Y, bar_w = 160;
    int progress = s_page_count > 0
        ? (s_current_page * 100) / (int)s_page_count
        : 0;
    epd_draw_rect(bar_x, bar_y, bar_w, 8, EPD_COLOR_BLACK);
    epd_fill_rect(bar_x + 2, bar_y + 2,
                  (bar_w - 4) * progress / 100, 4, EPD_COLOR_BLACK);

    char info[32];
    snprintf(info, sizeof(info), "%u/%u", (unsigned)(s_current_page + 1), (unsigned)s_page_count);
    int iw = widget_text_width(info);
    widget_draw_text((EPD_WIDTH - iw) / 2, PROGRESS_Y + 12, info, EPD_COLOR_BLACK);

    /* 局部/全刷交替：每 5 次全刷一次 */
    if (s_partial_count >= PARTIAL_CLEAN_INTERVAL) {
        epd_display_full();
        s_partial_count = 0;
    } else {
        epd_display_partial();
        s_partial_count++;
    }
}

static void on_key(KeyId key, KeyEvent event)
{
    if (key == KEY_PREV) {
        if (event == KEY_EVT_SHORT && s_current_page > 0) {
            s_current_page--;
            on_render();
        } else if (event == KEY_EVT_LONG && s_current_page >= 5) {
            s_current_page -= 5;
            on_render();
        }
    } else if (key == KEY_NEXT) {
        if (event == KEY_EVT_SHORT && s_current_page + 1 < s_page_count) {
            s_current_page++;
            on_render();
        } else if (event == KEY_EVT_LONG && s_current_page + 5 < s_page_count) {
            s_current_page += 5;
            on_render();
        }
    } else if (key == KEY_HOME && event == KEY_EVT_SHORT) {
        page_mgr_switch(PAGE_BOOKSHELF);
    }
}

const PageVtbl page_reader_vtbl = {
    .id = PAGE_READER,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

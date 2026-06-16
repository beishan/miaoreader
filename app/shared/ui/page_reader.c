/**
 * @file page_reader.c
 * @brief 阅读器页（共享层）— 上下文菜单 + 进度持久化
 */
#include "page_reader.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include "../mock/mock_books.h"
#include "../storage/book_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 分页参数 ──────────────────────────────────────── */
#define PAGE_LINES  14
#define LINE_HEIGHT 18
#define MARGIN_X    20
#define MARGIN_Y    25

/* ── 菜单项 ────────────────────────────────────────── */
static const char *s_menu_items[] = {
    "\xe2\x96\xb6 \xe7\xbb\xa7\xe7\xbb\xad\xe9\x98\x85\xe8\xaf\xbb",  /* ▶ 继续阅读 */
    "\xe2\x98\x85 \xe6\xb7\xbb\xe5\x8a\xa0\xe4\xb9\xa6\xe7\xad\xbe",  /* ★ 添加书签 */
    "\xe2\x84\xb9 \xe4\xb9\xa6\xe7\xb1\x8d\xe4\xbf\xa1\xe6\x81\xaf",  /* ℹ 书籍信息 */
};
#define MENU_COUNT 3

/* ── 状态变量 ──────────────────────────────────────── */
static int   s_book_index;
static char *s_book_text;              /* malloc 分配 */
static int   s_current_page;
static int   s_total_pages;
static char  s_book_name[256];         /* 文件名，用于存储 key */

static char  s_page_lines[PAGE_LINES][128];

static bool  s_menu_visible;
static int   s_menu_cursor;
static bool  s_show_info;

/* ──────────────────────────────────────────────────── */
/* 内部工具                                              */
/* ──────────────────────────────────────────────────── */

/**
 * @brief 遍历文本统计总页数
 */
static void calculate_pages(void)
{
    if (!s_book_text || s_book_text[0] == '\0') {
        s_total_pages = 1;
        return;
    }

    int newlines = 0;
    for (const char *p = s_book_text; *p; p++) {
        if (*p == '\n') newlines++;
    }
    s_total_pages = (newlines + PAGE_LINES - 1) / PAGE_LINES;
    if (s_total_pages < 1) s_total_pages = 1;
}

/**
 * @brief 跳到目标页，读取 PAGE_LINES 行到 s_page_lines
 */
static void load_page_lines(int page)
{
    memset(s_page_lines, 0, sizeof(s_page_lines));

    if (!s_book_text) return;

    /* 找到目标页首行的起始位置 */
    const char *p = s_book_text;
    int line_count = 0;
    while (*p && line_count < page * PAGE_LINES) {
        if (*p == '\n') line_count++;
        p++;
    }

    /* 读取 PAGE_LINES 行 */
    for (int i = 0; i < PAGE_LINES && *p; i++) {
        int len = 0;
        while (p[len] && p[len] != '\n' && len < (int)sizeof(s_page_lines[i]) - 1) {
            len++;
        }
        memcpy(s_page_lines[i], p, len);
        s_page_lines[i][len] = '\0';
        p += len;
        if (*p == '\n') p++;
    }
}

/* ──────────────────────────────────────────────────── */
/* 页面生命周期                                          */
/* ──────────────────────────────────────────────────── */

static void on_enter(void)
{
    /* 获取当前书籍索引 */
    MockBookMeta book;
    if (mock_books_get_current_reading(&book) == 0) {
        s_book_index = atoi(book.id);
    } else {
        s_book_index = 0;
    }

    /* 加载文本 */
    s_book_text = mock_books_load_text(s_book_index);

    /* 保存文件名用于存储 key */
    const char *fn = mock_books_get_filename(s_book_index);
    if (fn) {
        strncpy(s_book_name, fn, sizeof(s_book_name) - 1);
        s_book_name[sizeof(s_book_name) - 1] = '\0';
    } else {
        snprintf(s_book_name, sizeof(s_book_name), "book_%d", s_book_index);
    }

    /* 计算总页数 */
    calculate_pages();

    /* 恢复进度 */
    int saved = book_storage_load_progress(s_book_name);
    s_current_page = (saved > 0 && saved < s_total_pages) ? saved : 0;

    /* 加载当前页 */
    load_page_lines(s_current_page);

    s_menu_visible = false;
    s_menu_cursor  = 0;
    s_show_info    = false;
}

static void on_exit(void)
{
    /* 保存进度 */
    book_storage_save_progress(s_book_name, s_current_page);
    book_storage_save();

    /* 释放文本 */
    if (s_book_text) {
        free(s_book_text);
        s_book_text = NULL;
    }
}

/* ──────────────────────────────────────────────────── */
/* 渲染                                                  */
/* ──────────────────────────────────────────────────── */

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    /* ── 正文 ─────────────────────────────────────── */
    int y = MARGIN_Y;
    for (int i = 0; i < PAGE_LINES; i++) {
        if (s_page_lines[i][0] == '\0') break;
        widget_draw_text(MARGIN_X, y, s_page_lines[i], RENDERER_COLOR_BLACK);
        y += LINE_HEIGHT;
    }

    /* ── 进度条 ───────────────────────────────────── */
    int bar_x = MARGIN_X;
    int bar_y = RENDERER_HEIGHT - 28;
    int bar_w = RENDERER_WIDTH - 2 * MARGIN_X;
    int progress = (s_total_pages > 0)
                     ? (s_current_page * 100) / s_total_pages
                     : 0;

    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    if (progress > 0) {
        renderer_fill_rect(bar_x + 2, bar_y + 2,
                           (bar_w - 4) * progress / 100, 6,
                           RENDERER_COLOR_BLACK);
    }

    char page_str[32];
    snprintf(page_str, sizeof(page_str), "Page %d/%d (%d%%)",
             s_current_page + 1, s_total_pages, progress);
    int tw = widget_text_width(page_str);
    widget_draw_text((RENDERER_WIDTH - tw) / 2, bar_y + 14,
                     page_str, RENDERER_COLOR_BLACK);

    /* ── 菜单覆盖层 ──────────────────────────────── */
    if (s_menu_visible) {
        int mx = 80, my = 80, mw = 240, mh = 110;
        renderer_fill_rect(mx, my, mw, mh, RENDERER_COLOR_WHITE);
        renderer_draw_rect(mx, my, mw, mh, RENDERER_COLOR_BLACK);

        for (int i = 0; i < MENU_COUNT; i++) {
            RendererColor c = (i == s_menu_cursor)
                                ? RENDERER_COLOR_RED
                                : RENDERER_COLOR_BLACK;
            widget_draw_text(mx + 20, my + 20 + i * 30,
                             s_menu_items[i], c);
        }
    }

    /* ── 书籍信息弹窗 ────────────────────────────── */
    if (s_show_info) {
        int mx = 50, my = 60, mw = 300, mh = 140;
        renderer_fill_rect(mx, my, mw, mh, RENDERER_COLOR_WHITE);
        renderer_draw_rect(mx, my, mw, mh, RENDERER_COLOR_BLACK);

        int ty = my + 16;
        char buf[64];

        /* 书名 */
        widget_draw_text(mx + 12, ty, s_book_name, RENDERER_COLOR_BLACK);
        ty += 22;

        /* 总页数 */
        snprintf(buf, sizeof(buf), "Total pages: %d", s_total_pages);
        widget_draw_text(mx + 12, ty, buf, RENDERER_COLOR_BLACK);
        ty += 18;

        /* 当前页 / 百分比 */
        int pct = (s_total_pages > 0)
                    ? (s_current_page * 100) / s_total_pages
                    : 0;
        snprintf(buf, sizeof(buf), "Current: %d/%d (%d%%)",
                 s_current_page + 1, s_total_pages, pct);
        widget_draw_text(mx + 12, ty, buf, RENDERER_COLOR_BLACK);
        ty += 18;

        /* 书签数量 */
        int bm_pages[BOOKMARK_MAX_PER_BOOK];
        int bm_count = book_storage_get_bookmarks(s_book_name,
                                                   bm_pages,
                                                   BOOKMARK_MAX_PER_BOOK);
        snprintf(buf, sizeof(buf), "Bookmarks: %d", bm_count);
        widget_draw_text(mx + 12, ty, buf, RENDERER_COLOR_BLACK);
    }
}

/* ──────────────────────────────────────────────────── */
/* 按键处理                                              */
/* ──────────────────────────────────────────────────── */

static void on_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    /* ── 信息弹窗：任意键关闭 ────────────────────── */
    if (s_show_info) {
        if (key == KEY_HOME || key == KEY_PWR) {
            s_show_info = false;
            on_render();
        }
        return;
    }

    /* ── 菜单模式 ────────────────────────────────── */
    if (s_menu_visible) {
        switch (key) {
        case KEY_PREV:
            if (s_menu_cursor > 0) s_menu_cursor--;
            else                   s_menu_cursor = MENU_COUNT - 1;
            on_render();
            break;
        case KEY_NEXT:
            if (s_menu_cursor < MENU_COUNT - 1) s_menu_cursor++;
            else                                s_menu_cursor = 0;
            on_render();
            break;
        case KEY_HOME:
            switch (s_menu_cursor) {
            case 0: /* 继续阅读 */
                s_menu_visible = false;
                on_render();
                break;
            case 1: /* 添加书签 */
                book_storage_add_bookmark(s_book_name, s_current_page);
                book_storage_save();
                s_menu_visible = false;
                on_render();
                break;
            case 2: /* 书籍信息 */
                s_menu_visible = false;
                s_show_info    = true;
                on_render();
                break;
            }
            break;
        case KEY_PWR:
            s_menu_visible = false;
            on_render();
            break;
        default:
            break;
        }
        return;
    }

    /* ── 正常阅读模式 ────────────────────────────── */
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
        s_menu_visible = true;
        s_menu_cursor  = 0;
        on_render();
        break;
    case KEY_PWR:
        /* 退出前保存进度 */
        book_storage_save_progress(s_book_name, s_current_page);
        book_storage_save();
        page_mgr_switch(PAGE_BOOKSHELF);
        break;
    default:
        break;
    }
}

/* ──────────────────────────────────────────────────── */
/* 虚函数表                                              */
/* ──────────────────────────────────────────────────── */

const PageVtbl page_reader_vtbl = {
    .id        = PAGE_READER,
    .on_enter  = on_enter,
    .on_exit   = on_exit,
    .on_key    = on_key,
    .on_render = on_render,
};

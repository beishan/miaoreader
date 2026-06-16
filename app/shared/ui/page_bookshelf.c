/**
 * @file page_bookshelf.c
 * @brief 书架页（共享层）
 */
#include "page_bookshelf.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include "../mock/mock_books.h"
#include <stdio.h>

static int s_selected = 0;
static int s_book_count = 0;

static void on_enter(void)
{
    printf("[书架] 进入\n");
    s_book_count = mock_books_get_count();
    if (s_selected >= s_book_count) {
        s_selected = 0;
    }
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    if (s_book_count == 0) {
        widget_draw_text(60, 100, "No books", RENDERER_COLOR_BLACK);
        widget_draw_text(40, 130, "Put .txt/.epub files in", RENDERER_COLOR_BLACK);
        widget_draw_text(40, 150, "shared/books/", RENDERER_COLOR_BLACK);
        widget_draw_text(20, 280, "[PWR] Back", RENDERER_COLOR_BLACK);
        return;
    }

    /* 绘制 3x2 网格 */
    int cols = 3;
    int rows = (s_book_count + cols - 1) / cols;
    if (rows > 2) rows = 2;
    int display_count = s_book_count;
    if (display_count > 6) display_count = 6;

    for (int i = 0; i < display_count; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = 20 + col * 120;
        int y = 30 + row * 115;

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
            /* 截取书名前 6 个字符 */
            int len = 0;
            const char *p = book.title;
            while (*p && len < 6) {
                title[len++] = *p++;
            }
            title[len] = '\0';
            widget_draw_text(x + 10, y + 82, title, RENDERER_COLOR_BLACK);
        }
    }

    /* 底部信息 */
    char info[32];
    snprintf(info, sizeof(info), "Total: %d books", s_book_count);
    widget_draw_text(20, 260, info, RENDERER_COLOR_BLACK);

    /* 操作提示 */
    widget_draw_text(20, 280, "[HOME] Read  [PREV/NEXT] Select", RENDERER_COLOR_BLACK);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event == KEY_EVT_SHORT) {
        switch (key) {
        case KEY_PREV:
            if (s_selected > 0) s_selected--;
            on_render();
            break;
        case KEY_NEXT:
            if (s_selected < s_book_count - 1) s_selected++;
            on_render();
            break;
        case KEY_HOME:
            page_mgr_switch(PAGE_READER);
            break;
        case KEY_PWR:
            page_mgr_switch(PAGE_HOME);
            break;
        default:
            break;
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

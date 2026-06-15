/**
 * @file page_bookshelf.c
 * @brief 书架页（共享层）
 */
#include "page_bookshelf.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include <stdio.h>

static int s_selected = 0;

static void on_enter(void)
{
    printf("[书架] 进入\n");
    s_selected = 0;
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    /* 绘制 3x2 网格 */
    for (int i = 0; i < 6; i++) {
        int col = i % 3;
        int row = i / 3;
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
        char title[16];
        snprintf(title, sizeof(title), "Book %d", i + 1);
        widget_draw_text(x + 20, y + 82, title, RENDERER_COLOR_BLACK);
    }

    /* 翻页指示 */
    widget_draw_text(20, 260, "1/1  Total: 6 books", RENDERER_COLOR_BLACK);

    /* 提示 */
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
            if (s_selected < 5) s_selected++;
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

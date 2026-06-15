/**
 * @file page_reader.c
 * @brief 阅读器页（共享层）
 */
#include "page_reader.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include <stdio.h>

static int s_current_page = 0;
static int s_total_pages = 100;

static void on_enter(void)
{
    printf("[阅读器] 进入\n");
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int y = 25;

    /* 章节标题 */
    widget_draw_text(20, y, "Chapter 1: The Beginning", RENDERER_COLOR_BLACK);
    renderer_fill_rect(20, y + 18, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 内容 */
    widget_draw_text(20, y + 28, "It was a dark and stormy", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 46, "night. The wind howled", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 64, "through the trees as the", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 82, "rain beat against the old", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 100, "windows. Inside, a small", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 118, "lamp flickered on a worn", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 136, "wooden desk, casting long", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 154, "shadows across the room.", RENDERER_COLOR_BLACK);

    widget_draw_text(20, y + 182, "A young reader sat in the", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 200, "corner, completely lost in", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 218, "the pages of a book. The", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 236, "world outside faded away", RENDERER_COLOR_BLACK);

    /* 进度条 */
    int bar_x = 20, bar_y = 275, bar_w = RENDERER_WIDTH - 40;
    int progress = (s_current_page * 100) / s_total_pages;

    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 6, RENDERER_COLOR_BLACK);

    char page_str[32];
    snprintf(page_str, sizeof(page_str), "Page %d/%d (%d%%)",
             s_current_page + 1, s_total_pages, progress);
    int text_w = widget_text_width(page_str);
    widget_draw_text((RENDERER_WIDTH - text_w) / 2, bar_y + 14, page_str, RENDERER_COLOR_BLACK);

    /* 提示 */
    widget_draw_text(20, 295, "< PREV    ESC: Back    NEXT >", RENDERER_COLOR_BLACK);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event == KEY_EVT_SHORT) {
        switch (key) {
        case KEY_PREV:
            if (s_current_page > 0) s_current_page--;
            on_render();
            break;
        case KEY_NEXT:
            if (s_current_page < s_total_pages - 1) s_current_page++;
            on_render();
            break;
        case KEY_PWR:
        case KEY_HOME:
            page_mgr_switch(PAGE_BOOKSHELF);
            break;
        default:
            break;
        }
    }
}

const PageVtbl page_reader_vtbl = {
    .id = PAGE_READER,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

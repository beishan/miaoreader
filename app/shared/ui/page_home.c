/**
 * @file page_home.c
 * @brief 主页（共享层）
 */
#include "page_home.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include <stdio.h>

static void on_enter(void)
{
    printf("[主页] 进入\n");
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int y = 40;

    /* 天气 */
    widget_draw_text(20, y, "Weather: Sunny 25C", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 18, "Humidity: 55%", RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(20, y + 42, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 阅读统计 */
    widget_draw_text(20, y + 52, "Reading Stats:", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 70, "  Books: 5", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 88, "  Today: 38 min", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 106, "  Total: 127 hours", RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(20, y + 130, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 当前阅读 */
    widget_draw_text(20, y + 140, "Current Book:", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 158, "  The Three-Body Problem", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 176, "  Page 45 / 312 (14%)", RENDERER_COLOR_BLACK);

    /* 进度条 */
    int bar_x = 20, bar_y = y + 200, bar_w = RENDERER_WIDTH - 40;
    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * 14 / 100, 6, RENDERER_COLOR_BLACK);

    /* 提示 */
    widget_draw_text(120, 270, "[NEXT] Bookshelf", RENDERER_COLOR_RED);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event == KEY_EVT_SHORT) {
        if (key == KEY_NEXT) {
            page_mgr_switch(PAGE_BOOKSHELF);
        } else if (key == KEY_HOME) {
            page_mgr_switch(PAGE_SETTINGS);
        }
    }
}

const PageVtbl page_home_vtbl = {
    .id = PAGE_HOME,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

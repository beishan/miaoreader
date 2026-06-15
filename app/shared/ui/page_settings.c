/**
 * @file page_settings.c
 * @brief 设置页（共享层）
 */
#include "page_settings.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include <stdio.h>

static void on_enter(void)
{
    printf("[设置] 进入\n");
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int y = 30;

    widget_draw_text(20, y, "Settings", RENDERER_COLOR_BLACK);
    renderer_fill_rect(20, y + 20, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 阅读设置 */
    widget_draw_text(20, y + 30, "[Reading]", RENDERER_COLOR_RED);
    widget_draw_text(20, y + 50, "  Font: SourceHanSerif", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 68, "  Size: 20px", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 86, "  Line Spacing: 1.5", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 104, "  Char Spacing: 1px", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 122, "  Margin: 8px", RENDERER_COLOR_BLACK);

    renderer_fill_rect(20, y + 142, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 系统设置 */
    widget_draw_text(20, y + 152, "[System]", RENDERER_COLOR_RED);
    widget_draw_text(20, y + 172, "  Sleep: Clock", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 190, "  Timeout: 5 min", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 208, "  Rotation: 0", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 226, "  WiFi: Connected", RENDERER_COLOR_BLACK);
    widget_draw_text(20, y + 244, "  Version: v0.5.0", RENDERER_COLOR_BLACK);

    /* 提示 */
    widget_draw_text(20, 280, "[ESC] Back", RENDERER_COLOR_BLACK);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event == KEY_EVT_SHORT) {
        if (key == KEY_PWR) {
            page_mgr_switch(PAGE_HOME);
        }
    }
}

const PageVtbl page_settings_vtbl = {
    .id = PAGE_SETTINGS,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

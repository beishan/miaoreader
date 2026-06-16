/**
 * @file menu_bar.c
 * @brief 底部菜单栏（共享层）
 *
 * 三个标签：主页、书架、设置
 * PREV/NEXT 切换标签，HOME 确认
 */
#include "menu_bar.h"
#include "widget.h"
#include "renderer_if.h"
#include <stdio.h>

#define TAB_COUNT 3
#define TAB_WIDTH (RENDERER_WIDTH / TAB_COUNT)
#define BAR_Y     (RENDERER_HEIGHT - MENU_BAR_HEIGHT)

static const char *tab_labels[TAB_COUNT] = { " 主页 ", " 书架 ", " 设置 " };
static const PageId tab_pages[TAB_COUNT] = { PAGE_HOME, PAGE_BOOKSHELF, PAGE_SETTINGS };

static int s_current_tab = 0;

void menu_bar_init(void)
{
    s_current_tab = 0;
}

static int current_page_to_tab(void)
{
    PageId cur = page_mgr_current();
    for (int i = 0; i < TAB_COUNT; i++) {
        if (tab_pages[i] == cur) return i;
    }
    return s_current_tab;
}

void menu_bar_render(void)
{
    s_current_tab = current_page_to_tab();

    /* 底部分隔线 */
    renderer_fill_rect(0, BAR_Y, RENDERER_WIDTH, 1, RENDERER_COLOR_BLACK);

    for (int i = 0; i < TAB_COUNT; i++) {
        int x = i * TAB_WIDTH;
        int is_active = (i == s_current_tab);

        if (is_active) {
            /* 选中标签：黑底白字 */
            renderer_fill_rect(x, BAR_Y + 1, TAB_WIDTH, MENU_BAR_HEIGHT - 1, RENDERER_COLOR_BLACK);
            int tw = widget_text_width(tab_labels[i]);
            widget_draw_text(x + (TAB_WIDTH - tw) / 2, BAR_Y + 8, tab_labels[i], RENDERER_COLOR_WHITE);
        } else {
            /* 未选中标签：白底黑字 + 边框 */
            renderer_draw_rect(x, BAR_Y + 1, TAB_WIDTH, MENU_BAR_HEIGHT - 1, RENDERER_COLOR_BLACK);
            int tw = widget_text_width(tab_labels[i]);
            widget_draw_text(x + (TAB_WIDTH - tw) / 2, BAR_Y + 8, tab_labels[i], RENDERER_COLOR_BLACK);
        }

        /* 标签间分隔线 */
        if (i > 0) {
            renderer_fill_rect(x, BAR_Y + 1, 1, MENU_BAR_HEIGHT - 1, RENDERER_COLOR_BLACK);
        }
    }
}

void menu_bar_handle_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    if (key == KEY_PREV) {
        s_current_tab = (s_current_tab - 1 + TAB_COUNT) % TAB_COUNT;
        page_mgr_switch(tab_pages[s_current_tab]);
    } else if (key == KEY_NEXT) {
        s_current_tab = (s_current_tab + 1) % TAB_COUNT;
        page_mgr_switch(tab_pages[s_current_tab]);
    }
}

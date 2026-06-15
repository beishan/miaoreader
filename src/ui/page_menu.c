/**
 * @file page_menu.c
 * @brief 阅读上下文菜单：继续阅读/跳转/添加书签/书籍信息
 */
#include "page_menu.h"
#include "page_jump.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "engine/bookmark.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "page_menu";

#define MENU_ITEMS 4
#define MENU_Y_START 80
#define MENU_LINE_H 30

static const char *menu_labels[MENU_ITEMS] = {
    "▶ 继续阅读",
    "   跳转到...",
    "   添加书签",
    "   书籍信息",
};

/* 书籍信息（由 page_reader 设置） */
static char s_book_name[64] = {0};
static uint32_t s_current_page = 0;
static uint32_t s_total_pages = 0;
static int s_selected = 0;

void page_menu_set_book_info(const char *book_name, uint32_t current_page, uint32_t total_pages)
{
    if (book_name) {
        strncpy(s_book_name, book_name, sizeof(s_book_name) - 1);
        s_book_name[sizeof(s_book_name) - 1] = '\0';
    }
    s_current_page = current_page;
    s_total_pages = total_pages;
    s_selected = 0;
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入阅读菜单");
    s_selected = 0;
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    /* 标题 */
    const char *title = "阅读菜单";
    int tw = widget_text_width(title);
    widget_draw_text((EPD_WIDTH - tw) / 2, 40, title, EPD_COLOR_BLACK);

    /* 分割线 */
    epd_draw_rect(40, 60, EPD_WIDTH - 80, 1, EPD_COLOR_BLACK);

    /* 菜单项 */
    for (int i = 0; i < MENU_ITEMS; i++) {
        int y = MENU_Y_START + i * MENU_LINE_H;
        int color = (i == s_selected) ? EPD_COLOR_RED : EPD_COLOR_BLACK;

        char line[32];
        if (i == s_selected) {
            snprintf(line, sizeof(line), "> %s", menu_labels[i] + 3);
        } else {
            snprintf(line, sizeof(line), "  %s", menu_labels[i] + 3);
        }
        widget_draw_text(40, y, line, color);
    }

    /* 底部信息 */
    char info[64];
    snprintf(info, sizeof(info), "第 %lu/%lu 页", (unsigned long)(s_current_page + 1), (unsigned long)s_total_pages);
    int iw = widget_text_width(info);
    widget_draw_text((EPD_WIDTH - iw) / 2, EPD_HEIGHT - 30, info, EPD_COLOR_BLACK);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    switch (key) {
    case KEY_PREV:
        if (s_selected > 0) s_selected--;
        on_render();
        break;

    case KEY_NEXT:
        if (s_selected < MENU_ITEMS - 1) s_selected++;
        on_render();
        break;

    case KEY_HOME:
        switch (s_selected) {
        case 0: /* 继续阅读 */
            ESP_LOGI(TAG, "继续阅读");
            page_mgr_pop();
            break;

        case 1: /* 跳转到... */
            ESP_LOGI(TAG, "跳转到页码");
            page_jump_set_page_info(s_current_page, s_total_pages);
            page_mgr_push(PAGE_JUMP);
            break;

        case 2: /* 添加书签 */
            ESP_LOGI(TAG, "添加书签: %s 第 %lu 页", s_book_name, (unsigned long)s_current_page);
            bookmark_add(s_book_name, s_current_page);
            /* 显示提示后返回 */
            page_mgr_pop();
            break;

        case 3: /* 书籍信息 */
            ESP_LOGI(TAG, "书籍信息");
            /* TODO: 显示详细书籍信息 */
            page_mgr_pop();
            break;

        default:
            break;
        }
        break;

    case KEY_PWR:
        /* 返回阅读 */
        page_mgr_pop();
        break;

    default:
        break;
    }
}

const PageVtbl page_menu_vtbl = {
    .id = PAGE_MENU,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

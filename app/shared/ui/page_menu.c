#ifdef PC_SIMULATION

#include "page_menu.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "status_bar.h"
#include "mock_bookmark.h"
#include <stdio.h>
#include <string.h>

static char s_book_name[64] = {0};
static uint32_t s_current_page = 0;
static uint32_t s_total_pages = 0;
static int s_selected = 0;

static const char *menu_items[] = {
    "继续阅读",
    "跳转到...",
    "添加书签",
    "书籍信息",
};
#define MENU_ITEMS_COUNT 4

/* Forward declaration */
static void on_render(void);

void page_menu_set_book_info(const char *book_name, uint32_t current_page, uint32_t total_pages) {
    if (book_name) strncpy(s_book_name, book_name, 63);
    s_current_page = current_page;
    s_total_pages = total_pages;
    s_selected = 0;
}

static void on_enter(void) {
    s_selected = 0;
}

static void on_key(KeyId key, KeyEvent event) {
    if (event != KEY_EVT_SHORT) return;

    switch (key) {
        case KEY_PREV:
            if (s_selected > 0) {
                s_selected--;
                on_render();
            }
            break;
        case KEY_NEXT:
            if (s_selected < MENU_ITEMS_COUNT - 1) {
                s_selected++;
                on_render();
            }
            break;
        case KEY_HOME:
            switch (s_selected) {
                case 0: // 继续阅读
                    page_mgr_pop();
                    break;
                case 1: // 跳转到
                    page_mgr_push(PAGE_JUMP);
                    break;
                case 2: // 添加书签
                    mock_bookmark_add(s_book_name, s_current_page);
                    page_mgr_pop();
                    break;
                case 3: // 书籍信息
                    page_mgr_pop();
                    break;
            }
            break;
        case KEY_PWR:
            page_mgr_pop();
            break;
        default:
            break;
    }
}

static void on_render(void) {
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int cx = RENDERER_WIDTH / 2;

    // 标题
    const char *title = "阅读菜单";
    int tw = renderer_text_width(title);
    renderer_draw_text(cx - tw / 2, 40, title, RENDERER_COLOR_BLACK);

    // 分割线
    renderer_draw_rect(40, 60, RENDERER_WIDTH - 80, 1, RENDERER_COLOR_BLACK);

    // 菜单项
    int y = 80;
    for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
        char buf[80];
        if (i == s_selected) {
            snprintf(buf, sizeof(buf), "> %s", menu_items[i]);
            renderer_draw_text(60, y, buf, RENDERER_COLOR_RED);
        } else {
            snprintf(buf, sizeof(buf), "  %s", menu_items[i]);
            renderer_draw_text(60, y, buf, RENDERER_COLOR_BLACK);
        }
        y += 30;
    }

    // 底部页码
    char page_info[32];
    snprintf(page_info, sizeof(page_info), "第 %u/%u 页", s_current_page + 1, s_total_pages);
    int pw = renderer_text_width(page_info);
    renderer_draw_text(cx - pw / 2, RENDERER_HEIGHT - 30, page_info, RENDERER_COLOR_BLACK);

    renderer_display();
}

const PageVtbl page_menu_vtbl = {
    .id = PAGE_MENU,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#endif // PC_SIMULATION

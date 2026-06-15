/**
 * @file sim_main.c
 * @brief PC 仿真入口
 *
 * 在 PC 上运行电子书阅读器的 UI 仿真
 * 布局：状态栏（顶部）+ 主体区域（中间）+ 菜单栏（底部）
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 屏幕布局 */
#define STATUS_BAR_HEIGHT   24
#define MENU_BAR_HEIGHT     30
#define CONTENT_Y           STATUS_BAR_HEIGHT
#define CONTENT_H           (RENDERER_HEIGHT - STATUS_BAR_HEIGHT - MENU_BAR_HEIGHT)

/* 菜单项 */
typedef enum {
    MENU_HOME,
    MENU_BOOKSHELF,
    MENU_SETTINGS,
    MENU_COUNT,
} MenuItem;

/* 当前状态 */
static MenuItem s_current_menu = MENU_HOME;
static int s_selected_book = 0;
static int s_current_reader_page = 0;
static int s_total_reader_pages = 100;
static bool s_in_reader = false;

/* 绘制状态栏 */
static void draw_status_bar(void)
{
    renderer_fill_rect(0, 0, RENDERER_WIDTH, STATUS_BAR_HEIGHT, RENDERER_COLOR_BLACK);

    /* 时间 */
    renderer_draw_text(8, 5, "14:35", RENDERER_COLOR_WHITE);

    /* 日期 */
    renderer_draw_text(140, 5, "2025-06-15", RENDERER_COLOR_WHITE);

    /* 电量 */
    renderer_draw_text(RENDERER_WIDTH - 55, 5, "87%", RENDERER_COLOR_WHITE);
}

/* 绘制菜单栏 */
static void draw_menu_bar(void)
{
    int y = RENDERER_HEIGHT - MENU_BAR_HEIGHT;

    renderer_fill_rect(0, y, RENDERER_WIDTH, MENU_BAR_HEIGHT, RENDERER_COLOR_WHITE);
    renderer_fill_rect(0, y, RENDERER_WIDTH, 1, RENDERER_COLOR_BLACK);

    int item_width = RENDERER_WIDTH / MENU_COUNT;

    const char *labels[MENU_COUNT] = {
        "HOME",
        "SHELF",
        "SET",
    };

    for (int i = 0; i < MENU_COUNT; i++) {
        int x = i * item_width;
        int text_x = x + (item_width - renderer_text_width(labels[i])) / 2;

        if (i == (int)s_current_menu && !s_in_reader) {
            renderer_fill_rect(x + 2, y + 2, item_width - 4, MENU_BAR_HEIGHT - 4, RENDERER_COLOR_BLACK);
            renderer_draw_text(text_x, y + 8, labels[i], RENDERER_COLOR_WHITE);
        } else {
            renderer_draw_rect(x + 2, y + 2, item_width - 4, MENU_BAR_HEIGHT - 4, RENDERER_COLOR_BLACK);
            renderer_draw_text(text_x, y + 8, labels[i], RENDERER_COLOR_BLACK);
        }

        if (i < MENU_COUNT - 1) {
            renderer_fill_rect(x + item_width - 1, y + 4, 1, MENU_BAR_HEIGHT - 8, RENDERER_COLOR_BLACK);
        }
    }
}

/* 绘制主页内容 */
static void draw_home_content(void)
{
    int y = CONTENT_Y + 10;

    renderer_draw_text(20, y, "Weather: Sunny 25C", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 20, "Humidity: 55%", RENDERER_COLOR_BLACK);

    renderer_fill_rect(20, y + 45, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    renderer_draw_text(20, y + 55, "Reading Stats:", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 75, "  Books: 5", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 95, "  Today: 38 min", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 115, "  Total: 127 hours", RENDERER_COLOR_BLACK);

    renderer_fill_rect(20, y + 140, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    renderer_draw_text(20, y + 150, "Current Book:", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 170, "  The Three-Body Problem", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 190, "  Page 45 / 312 (14%)", RENDERER_COLOR_BLACK);

    int bar_x = 20, bar_y = y + 215, bar_w = RENDERER_WIDTH - 40;
    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * 14 / 100, 6, RENDERER_COLOR_BLACK);
}

/* 绘制书架内容 */
static void draw_bookshelf_content(void)
{
    int y = CONTENT_Y + 10;

    renderer_draw_text(20, y, "Bookshelf", RENDERER_COLOR_BLACK);

    for (int i = 0; i < 6; i++) {
        int col = i % 3;
        int row = i / 3;
        int x = 20 + col * 120;
        int book_y = y + 25 + row * 100;

        renderer_fill_rect(x, book_y, 100, 80, RENDERER_COLOR_WHITE);
        renderer_draw_rect(x, book_y, 100, 80, RENDERER_COLOR_BLACK);

        renderer_fill_rect(x + 30, book_y + 15, 40, 50, RENDERER_COLOR_BLACK);
        renderer_fill_rect(x + 32, book_y + 17, 36, 46, RENDERER_COLOR_WHITE);

        if (i == s_selected_book) {
            renderer_draw_rect(x - 2, book_y - 2, 104, 84, RENDERER_COLOR_RED);
        }

        char title[16];
        snprintf(title, sizeof(title), "Book %d", i + 1);
        renderer_draw_text(x + 20, book_y + 82, title, RENDERER_COLOR_BLACK);
    }

    renderer_draw_text(20, CONTENT_Y + CONTENT_H - 25, "1/1  Total: 6", RENDERER_COLOR_BLACK);
}

/* 绘制设置内容 */
static void draw_settings_content(void)
{
    int y = CONTENT_Y + 10;

    renderer_draw_text(20, y, "Settings", RENDERER_COLOR_BLACK);

    renderer_fill_rect(20, y + 20, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    renderer_draw_text(20, y + 30, "[Reading]", RENDERER_COLOR_RED);
    renderer_draw_text(20, y + 50, "  Font: SourceHanSerif", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 68, "  Size: 20px", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 86, "  Line Spacing: 1.5", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 104, "  Char Spacing: 1px", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 122, "  Margin: 8px", RENDERER_COLOR_BLACK);

    renderer_fill_rect(20, y + 142, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    renderer_draw_text(20, y + 152, "[System]", RENDERER_COLOR_RED);
    renderer_draw_text(20, y + 172, "  Sleep: Clock", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 190, "  Timeout: 5 min", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 208, "  Rotation: 0", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 226, "  WiFi: Connected", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, y + 244, "  Version: v0.5.0", RENDERER_COLOR_BLACK);
}

/* 绘制阅读器内容 */
static void draw_reader_content(void)
{
    int y = CONTENT_Y + 5;

    renderer_draw_text(20, y, "Chapter 1", RENDERER_COLOR_BLACK);
    renderer_fill_rect(20, y + 18, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    const char *lines[] = {
        "It was a dark and stormy",
        "night. The wind howled",
        "through the trees as the",
        "rain beat against the old",
        "windows. Inside, a small",
        "lamp flickered on a worn",
        "wooden desk, casting long",
        "shadows across the room.",
        "",
        "A young reader sat in the",
        "corner, completely lost in",
        "the pages of a book. The",
        "world outside faded away",
        "as the story unfolded...",
    };

    for (int i = 0; i < 14; i++) {
        if (lines[i][0] != '\0') {
            renderer_draw_text(20, y + 25 + i * 16, lines[i], RENDERER_COLOR_BLACK);
        }
    }

    int bar_x = 20, bar_y = CONTENT_Y + CONTENT_H - 30;
    int bar_w = RENDERER_WIDTH - 40;
    int progress = (s_current_reader_page * 100) / s_total_reader_pages;

    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 6, RENDERER_COLOR_BLACK);

    char page_str[32];
    snprintf(page_str, sizeof(page_str), "Page %d/%d (%d%%)",
             s_current_reader_page + 1, s_total_reader_pages, progress);
    int text_w = renderer_text_width(page_str);
    renderer_draw_text((RENDERER_WIDTH - text_w) / 2, bar_y + 14, page_str, RENDERER_COLOR_BLACK);
}

/* 渲染主体内容 */
static void draw_content(void)
{
    renderer_fill_rect(0, CONTENT_Y, RENDERER_WIDTH, CONTENT_H, RENDERER_COLOR_WHITE);
    renderer_fill_rect(0, CONTENT_Y, 1, CONTENT_H, RENDERER_COLOR_BLACK);
    renderer_fill_rect(RENDERER_WIDTH - 1, CONTENT_Y, 1, CONTENT_H, RENDERER_COLOR_BLACK);

    if (s_in_reader) {
        draw_reader_content();
    } else {
        switch (s_current_menu) {
        case MENU_HOME:
            draw_home_content();
            break;
        case MENU_BOOKSHELF:
            draw_bookshelf_content();
            break;
        case MENU_SETTINGS:
            draw_settings_content();
            break;
        default:
            break;
        }
    }
}

/* 完整渲染 */
static void render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();
    draw_content();
    draw_menu_bar();
    renderer_display();
}

/* 按键回调 */
static void on_key(RendererKeyId key, RendererKeyEvent event)
{
    if (event != RENDERER_KEY_EVT_SHORT) return;

    if (s_in_reader) {
        switch (key) {
        case RENDERER_KEY_PWR:
        case RENDERER_KEY_HOME:
            s_in_reader = false;
            break;
        case RENDERER_KEY_PREV:
            if (s_current_reader_page > 0) s_current_reader_page--;
            break;
        case RENDERER_KEY_NEXT:
            if (s_current_reader_page < s_total_reader_pages - 1) s_current_reader_page++;
            break;
        default:
            break;
        }
        render();
        return;
    }

    switch (key) {
    case RENDERER_KEY_PWR:
        break;

    case RENDERER_KEY_PREV:
        if (s_current_menu > 0) {
            s_current_menu--;
        } else {
            s_current_menu = MENU_COUNT - 1;
        }
        break;

    case RENDERER_KEY_NEXT:
        if (s_current_menu < MENU_COUNT - 1) {
            s_current_menu++;
        } else {
            s_current_menu = MENU_HOME;
        }
        break;

    case RENDERER_KEY_HOME:
        if (s_current_menu == MENU_BOOKSHELF) {
            s_in_reader = true;
            s_current_reader_page = 0;
        }
        break;

    default:
        break;
    }

    render();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== 妙阅 E-Reader PC 仿真 ===\n\n");
    printf("按键:\n");
    printf("  ESC   - 退出\n");
    printf("  LEFT  - 上一个菜单\n");
    printf("  RIGHT - 下一个菜单\n");
    printf("  ENTER - 确认/进入\n\n");

    if (renderer_init("妙阅 E-Reader 仿真") != 0) {
        fprintf(stderr, "渲染器初始化失败\n");
        return 1;
    }

    renderer_set_key_callback(on_key);
    render();

    printf("开始事件循环...\n");
    while (1) {
        if (renderer_poll_events() != 0) {
            break;
        }
        renderer_delay(16);
    }

    printf("退出仿真\n");
    return 0;
}

#endif /* PC_SIMULATION */

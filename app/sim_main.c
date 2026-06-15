/**
 * @file sim_main.c
 * @brief PC 仿真入口
 *
 * 在 PC 上运行电子书阅读器的 UI 仿真
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 模拟的页面状态 */
typedef enum {
    SIM_PAGE_HOME,
    SIM_PAGE_BOOKSHELF,
    SIM_PAGE_READER,
    SIM_PAGE_SETTINGS,
    SIM_PAGE_COUNT,
} SimPageId;

static SimPageId s_current_page = SIM_PAGE_HOME;
static int s_selected_book = 0;
static int s_current_reader_page = 0;
static int s_total_reader_pages = 100;

/* 绘制点阵字符（简化版，8x16） */
static void draw_char(int x, int y, char c, RendererColor color)
{
    /* 简化的字符绘制：用矩形模拟 */
    if (c == ' ') return;

    /* 对于数字和字母，绘制简单的矩形表示 */
    if (c >= '0' && c <= '9') {
        renderer_fill_rect(x + 2, y + 2, 12, 12, color);
    } else if (c >= 'A' && c <= 'Z') {
        renderer_fill_rect(x + 1, y + 1, 14, 14, color);
    } else {
        renderer_fill_rect(x + 3, y + 3, 10, 10, color);
    }
}

/* 绘制文本（简化版） */
static void draw_text(int x, int y, const char *text, RendererColor color)
{
    int cx = x;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            y += 18;
            cx = x;
            continue;
        }
        draw_char(cx, y, *p, color);
        cx += 16;
    }
}

/* 绘制状态栏 */
static void draw_status_bar(void)
{
    /* 黑底 */
    renderer_fill_rect(0, 0, RENDERER_WIDTH, 20, RENDERER_COLOR_BLACK);

    /* 时间 */
    draw_text(2, 2, "14:35", RENDERER_COLOR_WHITE);

    /* 电量 */
    draw_text(RENDERER_WIDTH - 60, 2, "87%", RENDERER_COLOR_WHITE);
}

/* 绘制主页 */
static void draw_home_page(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();

    /* 日期 */
    draw_text(100, 50, "2025-06-15", RENDERER_COLOR_BLACK);

    /* 天气 */
    draw_text(80, 80, "Sunny 25C", RENDERER_COLOR_BLACK);

    /* 统计 */
    draw_text(20, 150, "Books: 5", RENDERER_COLOR_BLACK);
    draw_text(20, 170, "Today: 38min", RENDERER_COLOR_BLACK);
    draw_text(20, 190, "Reading: Test", RENDERER_COLOR_BLACK);

    /* 提示 */
    draw_text(120, 270, "[NEXT] Shelf", RENDERER_COLOR_RED);
}

/* 绘制书架页 */
static void draw_bookshelf_page(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();

    /* 绘制 3x2 网格 */
    for (int i = 0; i < 6; i++) {
        int col = i % 3;
        int row = i / 3;
        int x = 20 + col * 120;
        int y = 30 + row * 130;

        /* 封面 */
        renderer_draw_rect(x, y, 100, 100, RENDERER_COLOR_BLACK);

        /* 选中框 */
        if (i == s_selected_book) {
            renderer_draw_rect(x - 2, y - 2, 104, 104, RENDERER_COLOR_RED);
        }

        /* 书名 */
        char title[16];
        snprintf(title, sizeof(title), "Book %d", i + 1);
        draw_text(x, y + 105, title, RENDERER_COLOR_BLACK);
    }

    /* 翻页指示 */
    draw_text(20, 280, "1/1  Total:6", RENDERER_COLOR_BLACK);
}

/* 绘制阅读器页 */
static void draw_reader_page(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();

    /* 模拟文本内容 */
    draw_text(20, 40, "Chapter 1", RENDERER_COLOR_BLACK);

    for (int i = 0; i < 10; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d content...", i + 1);
        draw_text(20, 70 + i * 20, line, RENDERER_COLOR_BLACK);
    }

    /* 进度条 */
    int bar_x = 120, bar_y = 286, bar_w = 160;
    int progress = (s_current_reader_page * 100) / s_total_reader_pages;
    renderer_draw_rect(bar_x, bar_y, bar_w, 8, RENDERER_COLOR_BLACK);
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 4, RENDERER_COLOR_BLACK);

    /* 页码 */
    char page_str[32];
    snprintf(page_str, sizeof(page_str), "%d/%d", s_current_reader_page + 1, s_total_reader_pages);
    draw_text(160, 296, page_str, RENDERER_COLOR_BLACK);
}

/* 绘制设置页 */
static void draw_settings_page(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();

    draw_text(20, 30, "[Reading Settings]", RENDERER_COLOR_RED);

    draw_text(20, 60, "> Font: SourceHan", RENDERER_COLOR_RED);
    draw_text(20, 82, "  Size: 20px", RENDERER_COLOR_BLACK);
    draw_text(20, 104, "  Line: 1.5", RENDERER_COLOR_BLACK);
    draw_text(20, 126, "  Char: 1px", RENDERER_COLOR_BLACK);
    draw_text(20, 148, "  Margin: 8px", RENDERER_COLOR_BLACK);

    draw_text(20, 180, "[System Settings]", RENDERER_COLOR_RED);

    draw_text(20, 210, "  Sleep: Clock", RENDERER_COLOR_BLACK);
    draw_text(20, 232, "  Rotation: 0", RENDERER_COLOR_BLACK);
    draw_text(20, 254, "  Timeout: 5min", RENDERER_COLOR_BLACK);
}

/* 渲染当前页面 */
static void render_current_page(void)
{
    switch (s_current_page) {
    case SIM_PAGE_HOME:
        draw_home_page();
        break;
    case SIM_PAGE_BOOKSHELF:
        draw_bookshelf_page();
        break;
    case SIM_PAGE_READER:
        draw_reader_page();
        break;
    case SIM_PAGE_SETTINGS:
        draw_settings_page();
        break;
    default:
        break;
    }

    renderer_display();
}

/* 按键回调 */
static void on_key(RendererKeyId key, RendererKeyEvent event)
{
    if (event != RENDERER_KEY_EVT_SHORT) return;

    switch (key) {
    case RENDERER_KEY_PWR:
        /* 返回主页 */
        s_current_page = SIM_PAGE_HOME;
        break;

    case RENDERER_KEY_PREV:
        if (s_current_page == SIM_PAGE_BOOKSHELF) {
            if (s_selected_book > 0) s_selected_book--;
        } else if (s_current_page == SIM_PAGE_READER) {
            if (s_current_reader_page > 0) s_current_reader_page--;
        } else if (s_current_page == SIM_PAGE_SETTINGS) {
            /* 上移菜单项 */
        }
        break;

    case RENDERER_KEY_NEXT:
        if (s_current_page == SIM_PAGE_HOME) {
            s_current_page = SIM_PAGE_BOOKSHELF;
        } else if (s_current_page == SIM_PAGE_BOOKSHELF) {
            if (s_selected_book < 5) s_selected_book++;
        } else if (s_current_page == SIM_PAGE_READER) {
            if (s_current_reader_page < s_total_reader_pages - 1) s_current_reader_page++;
        } else if (s_current_page == SIM_PAGE_SETTINGS) {
            /* 下移菜单项 */
        }
        break;

    case RENDERER_KEY_HOME:
        if (s_current_page == SIM_PAGE_BOOKSHELF) {
            s_current_page = SIM_PAGE_READER;
            s_current_reader_page = 0;
        } else if (s_current_page == SIM_PAGE_READER) {
            s_current_page = SIM_PAGE_BOOKSHELF;
        } else if (s_current_page == SIM_PAGE_HOME) {
            s_current_page = SIM_PAGE_SETTINGS;
        }
        break;

    default:
        break;
    }

    render_current_page();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== 妙阅 E-Reader PC 仿真 ===\n\n");

    /* 初始化渲染器 */
    if (renderer_init("妙阅 E-Reader 仿真") != 0) {
        fprintf(stderr, "渲染器初始化失败\n");
        return 1;
    }

    /* 设置按键回调 */
    renderer_set_key_callback(on_key);

    /* 渲染初始页面 */
    render_current_page();

    /* 事件循环 */
    printf("\n开始事件循环...\n");
    while (1) {
        if (renderer_poll_events() != 0) {
            break;
        }
        renderer_delay(16);  /* ~60fps */
    }

    /* 清理 */
    printf("退出仿真\n");
    renderer_sleep();

    return 0;
}

#endif /* PC_SIMULATION */

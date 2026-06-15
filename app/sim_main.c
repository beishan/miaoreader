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

/* 绘制状态栏 */
static void draw_status_bar(void)
{
    /* 黑底 */
    renderer_fill_rect(0, 0, RENDERER_WIDTH, 20, RENDERER_COLOR_BLACK);

    /* 时间 */
    renderer_draw_text(4, 4, "14:35", RENDERER_COLOR_WHITE);

    /* 电量 */
    renderer_draw_text(RENDERER_WIDTH - 50, 4, "87%", RENDERER_COLOR_WHITE);
}

/* 绘制主页 */
static void draw_home_page(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();

    /* 日期 */
    renderer_draw_text(120, 50, "2025-06-15", RENDERER_COLOR_BLACK);

    /* 天气 */
    renderer_draw_text(100, 80, "Sunny 25C", RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(20, 120, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 统计 */
    renderer_draw_text(20, 150, "Books: 5", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 170, "Today: 38min", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 190, "Reading: Test Book", RENDERER_COLOR_BLACK);

    /* WiFi 状态 */
    renderer_draw_text(20, 220, "WiFi: 192.168.1.100", RENDERER_COLOR_BLACK);

    /* 提示 */
    renderer_draw_text(120, 270, "[NEXT] Bookshelf", RENDERER_COLOR_RED);
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

        /* 书本图标 */
        renderer_fill_rect(x + 30, y + 25, 40, 50, RENDERER_COLOR_BLACK);
        renderer_fill_rect(x + 32, y + 27, 36, 46, RENDERER_COLOR_WHITE);

        /* 选中框 */
        if (i == s_selected_book) {
            renderer_draw_rect(x - 2, y - 2, 104, 104, RENDERER_COLOR_RED);
        }

        /* 书名 */
        char title[16];
        snprintf(title, sizeof(title), "Book %d", i + 1);
        renderer_draw_text(x + 10, y + 105, title, RENDERER_COLOR_BLACK);
    }

    /* 翻页指示 */
    renderer_draw_text(20, 280, "1/1  Total: 6", RENDERER_COLOR_BLACK);
}

/* 绘制阅读器页 */
static void draw_reader_page(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();

    /* 章节标题 */
    renderer_draw_text(20, 25, "Chapter 1: The Beginning", RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(20, 42, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 模拟文本内容 */
    const char *lines[] = {
        "It was a dark and stormy",
        "night. The wind howled",
        "through the trees, and",
        "the rain beat against",
        "the windows. Inside, a",
        "small lamp flickered on",
        "a wooden desk, casting",
        "long shadows across the",
        "room. A young reader sat",
        "quietly, lost in a book.",
    };

    for (int i = 0; i < 10; i++) {
        renderer_draw_text(20, 50 + i * 18, lines[i], RENDERER_COLOR_BLACK);
    }

    /* 进度条 */
    int bar_x = 100, bar_y = 240, bar_w = 200;
    int progress = (s_current_reader_page * 100) / s_total_reader_pages;
    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 6, RENDERER_COLOR_BLACK);

    /* 页码 */
    char page_str[32];
    snprintf(page_str, sizeof(page_str), "Page %d/%d (%d%%)",
             s_current_reader_page + 1, s_total_reader_pages, progress);
    renderer_draw_text(130, 260, page_str, RENDERER_COLOR_BLACK);

    /* 提示 */
    renderer_draw_text(20, 280, "< PREV", RENDERER_COLOR_BLACK);
    renderer_draw_text(RENDERER_WIDTH - 80, 280, "NEXT >", RENDERER_COLOR_BLACK);
}

/* 绘制设置页 */
static void draw_settings_page(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    draw_status_bar();

    renderer_draw_text(20, 25, "[Reading Settings]", RENDERER_COLOR_RED);

    /* 分割线 */
    renderer_fill_rect(20, 42, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    renderer_draw_text(20, 55, "> Font: SourceHanSerif", RENDERER_COLOR_RED);
    renderer_draw_text(20, 75, "  Size: 20px", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 95, "  Line: 1.5", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 115, "  Char: 1px", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 135, "  Margin: 8px", RENDERER_COLOR_BLACK);

    renderer_draw_text(20, 165, "[System Settings]", RENDERER_COLOR_RED);

    /* 分割线 */
    renderer_fill_rect(20, 182, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    renderer_draw_text(20, 195, "  Sleep: Clock+Weather", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 215, "  Rotation: 0 deg", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 235, "  Timeout: 5 min", RENDERER_COLOR_BLACK);
    renderer_draw_text(20, 255, "  Version: v0.5.0", RENDERER_COLOR_BLACK);

    /* 提示 */
    renderer_draw_text(20, 280, "[ESC] Back", RENDERER_COLOR_BLACK);
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
    printf("按 ESC 退出\n");
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

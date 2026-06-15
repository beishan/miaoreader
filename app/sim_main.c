/**
 * @file sim_main.c
 * @brief PC 仿真入口（使用共享页面层）
 *
 * 布局：状态栏（顶部）+ 主体区域（中间）+ 菜单栏（底部）
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include "shared/ui/page_mgr.h"
#include "shared/ui/page_home.h"
#include "shared/ui/page_bookshelf.h"
#include "shared/ui/page_reader.h"
#include "shared/ui/page_settings.h"
#include "shared/ui/status_bar.h"
#include "shared/ui/widget.h"
#include <stdio.h>

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

static MenuItem s_current_menu = MENU_HOME;
static bool s_in_reader = false;

/* 绘制菜单栏 */
static void draw_menu_bar(void)
{
    int y = RENDERER_HEIGHT - MENU_BAR_HEIGHT;

    renderer_fill_rect(0, y, RENDERER_WIDTH, MENU_BAR_HEIGHT, RENDERER_COLOR_WHITE);
    renderer_fill_rect(0, y, RENDERER_WIDTH, 1, RENDERER_COLOR_BLACK);

    int item_width = RENDERER_WIDTH / MENU_COUNT;

    const char *labels[MENU_COUNT] = {
        " 主页 ",
        " 书架 ",
        " 设置 ",
    };

    for (int i = 0; i < MENU_COUNT; i++) {
        int x = i * item_width;
        int text_x = x + (item_width - widget_text_width(labels[i])) / 2;

        if (i == (int)s_current_menu && !s_in_reader) {
            renderer_fill_rect(x + 2, y + 2, item_width - 4, MENU_BAR_HEIGHT - 4, RENDERER_COLOR_BLACK);
            widget_draw_text(text_x, y + 7, labels[i], RENDERER_COLOR_WHITE);
        } else {
            renderer_draw_rect(x + 2, y + 2, item_width - 4, MENU_BAR_HEIGHT - 4, RENDERER_COLOR_BLACK);
            widget_draw_text(text_x, y + 7, labels[i], RENDERER_COLOR_BLACK);
        }

        if (i < MENU_COUNT - 1) {
            renderer_fill_rect(x + item_width - 1, y + 4, 1, MENU_BAR_HEIGHT - 8, RENDERER_COLOR_BLACK);
        }
    }
}

/* 自定义渲染函数 */
static void custom_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);

    /* 状态栏 */
    renderer_fill_rect(0, 0, RENDERER_WIDTH, STATUS_BAR_HEIGHT, RENDERER_COLOR_BLACK);
    widget_draw_text(8, 5, "14:35", RENDERER_COLOR_WHITE);
    widget_draw_text(130, 5, "2025-06-15", RENDERER_COLOR_WHITE);
    widget_draw_text(RENDERER_WIDTH - 55, 5, "87%", RENDERER_COLOR_WHITE);

    /* 主体内容 */
    PageId current = page_mgr_current();

    if (s_in_reader) {
        /* 阅读器：全屏显示 */
        renderer_fill_rect(0, CONTENT_Y, RENDERER_WIDTH, RENDERER_HEIGHT - CONTENT_Y, RENDERER_COLOR_WHITE);

        int y = CONTENT_Y + 5;
        widget_draw_text(20, y, "第一章 深渊中的黑暗森林", RENDERER_COLOR_BLACK);
        renderer_fill_rect(20, y + 20, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

        widget_draw_text(20, y + 30, "这是一个黑暗而狂风暴雨的夜晚。", RENDERER_COLOR_BLACK);
        widget_draw_text(20, y + 50, "风在树林间呼啸，雨点敲打着", RENDERER_COLOR_BLACK);
        widget_draw_text(20, y + 70, "古老的窗户。屋内，一盏小灯", RENDERER_COLOR_BLACK);
        widget_draw_text(20, y + 90, "在破旧的书桌上闪烁，投下长长", RENDERER_COLOR_BLACK);
        widget_draw_text(20, y + 110, "的影子。一个年轻的读者坐在", RENDERER_COLOR_BLACK);
        widget_draw_text(20, y + 130, "角落里，完全沉浸在书本中。", RENDERER_COLOR_BLACK);

        widget_draw_text(20, y + 160, "外面的世界渐渐远去，故事", RENDERER_COLOR_BLACK);
        widget_draw_text(20, y + 180, "在眼前展开...", RENDERER_COLOR_BLACK);

        /* 进度条 */
        int bar_x = 20, bar_y = RENDERER_HEIGHT - 30;
        int bar_w = RENDERER_WIDTH - 40;
        renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
        renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * 10 / 100, 6, RENDERER_COLOR_BLACK);
        widget_draw_text(160, bar_y + 14, "第 1/100 页 (1%)", RENDERER_COLOR_BLACK);

    } else {
        /* 非阅读器：显示内容区域 + 菜单栏 */
        renderer_fill_rect(0, CONTENT_Y, RENDERER_WIDTH, CONTENT_H, RENDERER_COLOR_WHITE);
        renderer_fill_rect(0, CONTENT_Y, 1, CONTENT_H, RENDERER_COLOR_BLACK);
        renderer_fill_rect(RENDERER_WIDTH - 1, CONTENT_Y, 1, CONTENT_H, RENDERER_COLOR_BLACK);

        switch (current) {
        case PAGE_HOME:
            widget_draw_text(20, CONTENT_Y + 10, "天气：晴 25°C", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 30, "湿度：55%", RENDERER_COLOR_BLACK);
            renderer_fill_rect(20, CONTENT_Y + 55, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 65, "阅读统计", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 85, "  藏书：5 册", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 105, "  今日：38 分钟", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 125, "  累计：127 小时", RENDERER_COLOR_BLACK);
            renderer_fill_rect(20, CONTENT_Y + 150, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 160, "正在阅读", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 180, "  三体", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 200, "  第 45/312 页 (14%)", RENDERER_COLOR_BLACK);
            break;

        case PAGE_BOOKSHELF:
            widget_draw_text(20, CONTENT_Y + 10, "书架", RENDERER_COLOR_BLACK);
            for (int i = 0; i < 6; i++) {
                int col = i % 3;
                int row = i / 3;
                int x = 20 + col * 120;
                int y = CONTENT_Y + 30 + row * 100;
                renderer_draw_rect(x, y, 100, 80, RENDERER_COLOR_BLACK);
                renderer_fill_rect(x + 30, y + 15, 40, 50, RENDERER_COLOR_BLACK);
                renderer_fill_rect(x + 32, y + 17, 36, 46, RENDERER_COLOR_WHITE);
                char title[16];
                snprintf(title, sizeof(title), "书籍 %d", i + 1);
                widget_draw_text(x + 15, y + 82, title, RENDERER_COLOR_BLACK);
            }
            widget_draw_text(20, CONTENT_Y + CONTENT_H - 25, "第 1/1 页  共 6 本", RENDERER_COLOR_BLACK);
            break;

        case PAGE_SETTINGS:
            widget_draw_text(20, CONTENT_Y + 10, "设置", RENDERER_COLOR_BLACK);
            renderer_fill_rect(20, CONTENT_Y + 30, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 40, "阅读排版", RENDERER_COLOR_RED);
            widget_draw_text(20, CONTENT_Y + 60, "  字体：思源宋体", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 80, "  字号：20px", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 100, "  行距：1.5", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 120, "  字距：1px", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 140, "  页边距：8px", RENDERER_COLOR_BLACK);
            renderer_fill_rect(20, CONTENT_Y + 160, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 170, "系统设置", RENDERER_COLOR_RED);
            widget_draw_text(20, CONTENT_Y + 190, "  休眠：时钟天气", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 210, "  超时：5 分钟", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 230, "  版本：v0.5.0", RENDERER_COLOR_BLACK);
            break;

        default:
            break;
        }

        /* 菜单栏 */
        draw_menu_bar();
    }

    renderer_display();
}

/* 按键回调 */
static void on_key(RendererKeyId key, RendererKeyEvent event)
{
    if (event != RENDERER_KEY_EVT_SHORT) return;

    /* 阅读器模式 */
    if (s_in_reader) {
        if (key == RENDERER_KEY_PWR || key == RENDERER_KEY_HOME) {
            s_in_reader = false;
        }
        custom_render();
        return;
    }

    /* 菜单模式 */
    switch (key) {
    case RENDERER_KEY_PWR:
        break;

    case RENDERER_KEY_PREV:
        if (s_current_menu > 0) {
            s_current_menu--;
        } else {
            s_current_menu = MENU_COUNT - 1;
        }
        page_mgr_switch((PageId)s_current_menu);
        break;

    case RENDERER_KEY_NEXT:
        if (s_current_menu < MENU_COUNT - 1) {
            s_current_menu++;
        } else {
            s_current_menu = MENU_HOME;
        }
        page_mgr_switch((PageId)s_current_menu);
        break;

    case RENDERER_KEY_HOME:
        if (s_current_menu == MENU_BOOKSHELF) {
            s_in_reader = true;
        } else if (s_current_menu == MENU_HOME) {
            s_current_menu = MENU_SETTINGS;
            page_mgr_switch(PAGE_SETTINGS);
        }
        break;

    default:
        break;
    }

    custom_render();
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

    /* 初始化渲染器 */
    if (renderer_init("妙阅 E-Reader 仿真") != 0) {
        fprintf(stderr, "渲染器初始化失败\n");
        return 1;
    }

    /* 初始化页面管理器 */
    page_mgr_init();
    widget_init();
    status_bar_init();

    /* 注册页面 */
    page_mgr_register(&page_home_vtbl);
    page_mgr_register(&page_bookshelf_vtbl);
    page_mgr_register(&page_reader_vtbl);
    page_mgr_register(&page_settings_vtbl);

    /* 设置按键回调 */
    renderer_set_key_callback(on_key);

    /* 进入主页 */
    page_mgr_switch(PAGE_HOME);

    /* 初始渲染 */
    custom_render();

    /* 事件循环 */
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

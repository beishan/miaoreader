/**
 * @file sim_main.c
 * @brief PC 仿真入口（委托 PageVtbl 系统）
 *
 * 渲染和按键完全委托给页面状态机，不再有双轨逻辑。
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include "shared/ui/page_mgr.h"
#include "shared/ui/page_boot.h"
#include "shared/ui/page_home.h"
#include "shared/ui/page_bookshelf.h"
#include "shared/ui/page_reader.h"
#include "shared/ui/page_settings.h"
#include "shared/ui/page_menu.h"
#include "shared/ui/page_jump.h"
#include "shared/ui/page_sleep.h"
#include "shared/ui/status_bar.h"
#include "shared/ui/menu_bar.h"
#include "shared/ui/widget.h"
#include "shared/mock/mock_rtc.h"
#include "shared/mock/mock_weather.h"
#include "shared/mock/mock_books.h"
#include "shared/mock/mock_wifi.h"
#include "shared/storage/book_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 判断当前页面是否显示菜单栏 */
static int should_show_menu_bar(void)
{
    PageId cur = page_mgr_current();
    return (cur == PAGE_HOME || cur == PAGE_BOOKSHELF || cur == PAGE_SETTINGS);
}

/* 自定义渲染函数：委托给当前页面的 on_render + 菜单栏 */
static void custom_render(void)
{
    const PageVtbl *page = page_mgr_get_current_vtbl();
    if (page && page->on_render) {
        page->on_render();
    }
    if (should_show_menu_bar()) {
        menu_bar_render();
    }
    renderer_display();
}

/* 按键回调：映射键码后委托给 page_mgr_handle_key */
static void on_key(RendererKeyId key, RendererKeyEvent event)
{
    /* 映射 RendererKeyId → KeyId */
    KeyId page_key;
    switch (key) {
    case RENDERER_KEY_PWR:  page_key = KEY_PWR;  break;
    case RENDERER_KEY_PREV: page_key = KEY_PREV; break;
    case RENDERER_KEY_NEXT: page_key = KEY_NEXT; break;
    case RENDERER_KEY_HOME: page_key = KEY_HOME; break;
    default: return;
    }

    /* 映射 RendererKeyEvent → KeyEvent */
    KeyEvent page_event;
    switch (event) {
    case RENDERER_KEY_EVT_SHORT:      page_event = KEY_EVT_SHORT;      break;
    case RENDERER_KEY_EVT_LONG:       page_event = KEY_EVT_LONG;       break;
    case RENDERER_KEY_EVT_SUPER_LONG: page_event = KEY_EVT_SUPER_LONG; break;
    case RENDERER_KEY_EVT_COMBO:      page_event = KEY_EVT_COMBO;      break;
    default: return;
    }

    /* 菜单栏页面优先处理 PREV/NEXT */
    if (should_show_menu_bar() && (page_key == KEY_PREV || page_key == KEY_NEXT)) {
        menu_bar_handle_key(page_key, page_event);
        return;
    }

    /* 分发到页面状态机 */
    page_mgr_handle_key(page_key, page_event);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== 妙阅 E-Reader PC 仿真 ===\n\n");
    printf("按键:\n");
    printf("  ESC   - 退出选书模式 / 退出阅读 / 退出仿真\n");
    printf("  LEFT  - 上一个标签 / 上一本书 / 菜单上移\n");
    printf("  RIGHT - 下一个标签 / 下一本书 / 菜单下移\n");
    printf("  ENTER - 确认进入书架 / 打开书籍 / 打开菜单\n\n");

    /* 初始化渲染器 */
    if (renderer_init("妙阅 E-Reader 仿真") != 0) {
        fprintf(stderr, "渲染器初始化失败\n");
        return 1;
    }

    /* 初始化 Mock 数据层 */
    mock_rtc_init();
    mock_weather_init();
    book_storage_init();
    mock_books_init();
    mock_wifi_init();

    /* 初始化页面管理器 */
    page_mgr_init();
    widget_init();
    status_bar_init();
    menu_bar_init();

    /* 注册页面 */
    page_mgr_register(&page_boot_vtbl);
    page_mgr_register(&page_home_vtbl);
    page_mgr_register(&page_bookshelf_vtbl);
    page_mgr_register(&page_reader_vtbl);
    page_mgr_register(&page_settings_vtbl);
    page_mgr_register(&page_menu_vtbl);
    page_mgr_register(&page_jump_vtbl);
    page_mgr_register(&page_sleep_vtbl);

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

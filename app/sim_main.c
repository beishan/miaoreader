/**
 * @file sim_main.c
 * @brief PC 仿真入口（使用共享页面层）
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

/* 按键回调 */
static void on_key(RendererKeyId key, RendererKeyEvent event)
{
    if (event != RENDERER_KEY_EVT_SHORT) return;

    /* 映射到页面管理器的按键 */
    KeyId mapped_key;
    switch (key) {
    case RENDERER_KEY_PWR:  mapped_key = KEY_PWR; break;
    case RENDERER_KEY_PREV: mapped_key = KEY_PREV; break;
    case RENDERER_KEY_NEXT: mapped_key = KEY_NEXT; break;
    case RENDERER_KEY_HOME: mapped_key = KEY_HOME; break;
    default: return;
    }

    page_mgr_handle_key(mapped_key, KEY_EVT_SHORT);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== 妙阅 E-Reader PC 仿真 ===\n\n");
    printf("使用共享页面层\n\n");
    printf("按键:\n");
    printf("  ESC   - PWR (电源/返回)\n");
    printf("  LEFT  - PREV (上一页)\n");
    printf("  RIGHT - NEXT (下一页/进入书架)\n");
    printf("  ENTER - HOME (确认/菜单)\n\n");

    /* 初始化渲染器 */
    if (renderer_init("妙阅 E-Reader 仿真") != 0) {
        fprintf(stderr, "渲染器初始化失败\n");
        return 1;
    }

    /* 初始化页面管理器 */
    page_mgr_init();

    /* 初始化 widget */
    widget_init();

    /* 初始化状态栏 */
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

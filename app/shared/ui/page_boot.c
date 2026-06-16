/**
 * @file page_boot.c
 * @brief 首次配网引导页（PC 仿真）
 *
 * 显示 WiFi 配网提示，任意键跳过进入主页。
 */
#ifdef PC_SIMULATION

#include "page_boot.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "status_bar.h"
#include <stdio.h>
#include <string.h>

static void on_enter(void)
{
    printf("[Boot] 进入配网引导页\n");
}

static void on_key(KeyId key, KeyEvent event)
{
    (void)key;
    if (event != KEY_EVT_SHORT) return;

    /* 任意键跳过配网，进入主页 */
    page_mgr_switch(PAGE_HOME);
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int cx = RENDERER_WIDTH / 2;

    /* 标题 */
    const char *title = "欢迎使用 妙阅";
    int tw = widget_text_width(title);
    widget_draw_text(cx - tw / 2, 40, title, RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(40, 60, RENDERER_WIDTH - 80, 1, RENDERER_COLOR_BLACK);

    /* 配网提示 */
    const char *lines[] = {
        "请连接以下 WiFi 进行配网:",
        "SSID: EReader-SIM",
        "密码: ereader123",
        "然后访问 http://192.168.4.1",
        "配置 WiFi 和天气信息",
    };
    int y = 80;
    for (int i = 0; i < 5; i++) {
        int lw = widget_text_width(lines[i]);
        widget_draw_text(cx - lw / 2, y, lines[i], RENDERER_COLOR_BLACK);
        y += 25;
    }

    /* 底部提示 */
    const char *hint = "按任意键跳过配网";
    int hw = widget_text_width(hint);
    widget_draw_text(cx - hw / 2, RENDERER_HEIGHT - 30, hint, RENDERER_COLOR_BLACK);

    renderer_display();
}

const PageVtbl page_boot_vtbl = {
    .id = PAGE_BOOT,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#endif /* PC_SIMULATION */

/**
 * @file status_bar.c
 * @brief 状态栏（共享层）
 */
#include "status_bar.h"
#include "widget.h"
#include "renderer_if.h"
#include "../mock/mock_rtc.h"
#include "../mock/mock_wifi.h"
#include <stdio.h>

#define STATUS_BAR_HEIGHT 20

esp_err_t status_bar_init(void)
{
    return ESP_OK;
}

void status_bar_render(void)
{
    char time_buf[16];

    /* 黑底 */
    renderer_fill_rect(0, 0, RENDERER_WIDTH, STATUS_BAR_HEIGHT, RENDERER_COLOR_BLACK);

    /* 时间（使用 mock RTC） */
    mock_rtc_get_time_str(time_buf, sizeof(time_buf));
    widget_draw_text(4, 2, time_buf, RENDERER_COLOR_WHITE);

    /* 电量 */
    widget_draw_text(RENDERER_WIDTH - 50, 2, "87%", RENDERER_COLOR_WHITE);

    /* WiFi 图标 */
    uint16_t wifi_icon = mock_wifi_is_connected() ? ICON_WIFI_CONNECTED : ICON_WIFI_DISCONNECTED;
    widget_draw_icon(RENDERER_WIDTH - 20, 2, wifi_icon, RENDERER_COLOR_WHITE);
}

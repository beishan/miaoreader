/**
 * @file page_boot.c
 * @brief 首次配网引导页：显示 AP 配网信息
 */
#include "page_boot.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "hal/wifi_mgr.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "page_boot";

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入配网引导页");
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    /* 标题 */
    const char *title = "欢迎使用 妙阅";
    int tw = widget_text_width(title);
    widget_draw_text((EPD_WIDTH - tw) / 2, 40, title, EPD_COLOR_BLACK);

    /* 分割线 */
    epd_draw_rect(40, 60, EPD_WIDTH - 80, 1, EPD_COLOR_BLACK);

    /* 生成 AP SSID */
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "EReader-%02X%02X", mac[4], mac[5]);

    /* 配网信息 */
    int y = 90;
    char line[64];

    snprintf(line, sizeof(line), "请连接以下 WiFi 进行配网:");
    int lw = widget_text_width(line);
    widget_draw_text((EPD_WIDTH - lw) / 2, y, line, EPD_COLOR_BLACK);
    y += 30;

    snprintf(line, sizeof(line), "SSID: %s", ap_ssid);
    lw = widget_text_width(line);
    widget_draw_text((EPD_WIDTH - lw) / 2, y, line, EPD_COLOR_BLACK);
    y += 25;

    snprintf(line, sizeof(line), "密码: ereader123");
    lw = widget_text_width(line);
    widget_draw_text((EPD_WIDTH - lw) / 2, y, line, EPD_COLOR_BLACK);
    y += 30;

    snprintf(line, sizeof(line), "然后访问 http://192.168.4.1");
    lw = widget_text_width(line);
    widget_draw_text((EPD_WIDTH - lw) / 2, y, line, EPD_COLOR_BLACK);
    y += 25;

    snprintf(line, sizeof(line), "配置 WiFi 和天气信息");
    lw = widget_text_width(line);
    widget_draw_text((EPD_WIDTH - lw) / 2, y, line, EPD_COLOR_BLACK);

    /* 底部提示 */
    const char *hint = "配置完成后设备将自动重启";
    int hw = widget_text_width(hint);
    widget_draw_text((EPD_WIDTH - hw) / 2, EPD_HEIGHT - 30, hint, EPD_COLOR_BLACK);
}

static void on_key(KeyId key, KeyEvent event)
{
    /* 配网页面不响应按键，等待网页配置完成重启 */
}

const PageVtbl page_boot_vtbl = {
    .id = PAGE_BOOT,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

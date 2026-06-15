/**
 * @file status_bar.c
 * @brief 状态栏实现：20px 黑底白字，时间左对齐 + 电量/WiFi 右对齐
 */
#include "status_bar.h"
#include "widget.h"
#include "engine/event_bus.h"
#include "engine/types.h"
#include "hal/battery.h"
#include "hal/rtc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "status_bar";

/* 电量/连接状态缓存（事件触发刷新） */
static bool s_wifi_connected = false;
static int  s_wifi_rssi = 0;

/* 定时器：每分钟更新一次时间显示 */
static esp_timer_handle_t s_time_timer = NULL;

/* 事件处理：仅响应电量和 WiFi 变化 */
static void on_event(EventType type, void *data)
{
    if (type == EVT_BATTERY) {
        status_bar_update_battery();
    } else if (type == EVT_WIFI_STATUS) {
        if (data) {
            s_wifi_connected = *(bool *)data;
        }
        status_bar_update_wifi(s_wifi_connected, s_wifi_rssi);
    }
}

static void time_timer_cb(void *arg)
{
    (void)arg;
    status_bar_update_time();
}

esp_err_t status_bar_init(void)
{
    ESP_LOGI(TAG, "初始化状态栏");
    event_bus_subscribe(EVT_BATTERY, on_event);
    event_bus_subscribe(EVT_WIFI_STATUS, on_event);

    /* 1 分钟定时器刷新时间 */
    esp_timer_create_args_t tcfg = {
        .callback = time_timer_cb,
        .name = "status_time",
    };
    esp_err_t err = esp_timer_create(&tcfg, &s_time_timer);
    if (err == ESP_OK) {
        esp_timer_start_periodic(s_time_timer, 60 * 1000 * 1000);
    }
    return ESP_OK;
}

/* 渲染主流程：背景 + 时间 + 充电图标 + 百分比 + WiFi 图标 */
static void render_full_bar(void)
{
    /* 黑底 */
    epd_fill_rect(0, 0, EPD_WIDTH, STATUS_BAR_HEIGHT, EPD_COLOR_BLACK);

    /* 时间（左侧，y=2 居中于 20px） */
    char time_str[8] = "--:--";
    struct tm t;
    if (ds3231_get_time(&t) == ESP_OK) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", t.tm_hour, t.tm_min);
    }
    widget_draw_text(2, 2, time_str, EPD_COLOR_WHITE);

    /* 右侧图标：充电 -> 百分比 -> WiFi（从右往左累加） */
    int rx = EPD_WIDTH - 2;   /* 右边起点 */
    uint8_t pct = battery_get_percent();
    bool charging = battery_is_charging();
    bool low = battery_is_low();

    /* WiFi 图标（最右） */
    uint16_t wifi_icon = s_wifi_connected ? ICON_WIFI_CONNECTED : ICON_WIFI_DISCONNECTED;
    rx -= 16;
    widget_draw_icon(rx, 2, wifi_icon, EPD_COLOR_WHITE);

    /* 充电图标（可选） */
    if (charging) {
        rx -= 18;  /* 16 + 2 间距 */
        widget_draw_icon(rx, 2, ICON_CHARGING, EPD_COLOR_WHITE);
    }

    /* 百分比文本 */
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%u%%", pct);
    int pct_w = widget_text_width(pct_str);
    rx -= pct_w;
    widget_draw_text(rx, 2, pct_str, low ? EPD_COLOR_RED : EPD_COLOR_WHITE);
}

void status_bar_update_time(void)
{
    render_full_bar();
}

void status_bar_update_battery(void)
{
    render_full_bar();
}

void status_bar_update_wifi(bool connected, int rssi)
{
    s_wifi_connected = connected;
    s_wifi_rssi = rssi;
    render_full_bar();
}

void status_bar_render(void)
{
    render_full_bar();
}

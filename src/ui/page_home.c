/**
 * @file page_home.c
 * @brief 主页：日期 + 天气 + 阅读统计 + 进入书架
 */
#include "page_home.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "hal/rtc.h"
#include "hal/wifi_mgr.h"
#include "net/weather.h"
#include "engine/config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "page_home";

static const char *weekday_cn(int wday)
{
    static const char *days[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    if (wday < 0 || wday > 6) return "--";
    return days[wday];
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入主页");
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    /* 日期区：y=40 居中 */
    struct tm t;
    char date_str[64] = "-- 周- 1970年01月01日";
    if (ds3231_get_time(&t) == ESP_OK) {
        snprintf(date_str, sizeof(date_str), "%s %d年%02d月%02d日",
                 weekday_cn(t.tm_wday),
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }
    int w = widget_text_width(date_str);
    widget_draw_text((EPD_WIDTH - w) / 2, 40, date_str, EPD_COLOR_BLACK);

    /* 天气区：y=70 */
    WeatherData weather;
    if (weather_get_current(&weather) == ESP_OK && weather.temp_now != 0) {
        /* 城市 + 天气状况 */
        char weather_str[80];
        snprintf(weather_str, sizeof(weather_str), "%s  %s",
                 weather.city, weather.condition);
        int ww = widget_text_width(weather_str);
        widget_draw_text((EPD_WIDTH - ww) / 2, 70, weather_str, EPD_COLOR_BLACK);

        /* 温度 */
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), "%d°C  %d/%d°C",
                 weather.temp_now, weather.temp_low, weather.temp_high);
        int tw = widget_text_width(temp_str);
        widget_draw_text((EPD_WIDTH - tw) / 2, 90, temp_str, EPD_COLOR_BLACK);

        /* 湿度 + 空气质量 */
        char detail_str[32];
        snprintf(detail_str, sizeof(detail_str), "湿度:%d%%  AQI:%s",
                 weather.humidity, weather.aqi[0] ? weather.aqi : "--");
        int dw = widget_text_width(detail_str);
        widget_draw_text((EPD_WIDTH - dw) / 2, 110, detail_str, EPD_COLOR_BLACK);
    } else {
        /* 天气未配置 */
        const char *no_weather = "天气未配置";
        int nw = widget_text_width(no_weather);
        widget_draw_text((EPD_WIDTH - nw) / 2, 80, no_weather, EPD_COLOR_BLACK);

        /* WiFi 状态提示 */
        const char *wifi_hint = wifi_mgr_is_connected()
            ? "请在网页端配置天气 API"
            : "请连接 WiFi 或网页配网";
        int hw = widget_text_width(wifi_hint);
        widget_draw_text((EPD_WIDTH - hw) / 2, 100, wifi_hint, EPD_COLOR_BLACK);
    }

    /* 分割线 y=130 */
    epd_draw_rect(20, 130, EPD_WIDTH - 40, 1, EPD_COLOR_BLACK);

    /* 统计区 y=150..220 */
    widget_draw_text(20, 150, "藏书: -- 册", EPD_COLOR_BLACK);
    widget_draw_text(20, 170, "今日阅读: -- 分钟", EPD_COLOR_BLACK);
    widget_draw_text(20, 190, "正在读: --", EPD_COLOR_BLACK);

    /* WiFi 状态 */
    char ip_str[32];
    if (wifi_mgr_is_connected()) {
        wifi_mgr_get_ip(ip_str + 5, sizeof(ip_str) - 5);
        memcpy(ip_str, "IP: ", 4);
    } else {
        snprintf(ip_str, sizeof(ip_str), "WiFi: 未连接");
    }
    widget_draw_text(20, 220, ip_str, EPD_COLOR_BLACK);

    /* 提示：按 NEXT 进入书架 */
    static const char *hint = "[NEXT] 书架";
    int hhw = widget_text_width(hint);
    widget_draw_text((EPD_WIDTH - hhw) / 2, 270, hint, EPD_COLOR_RED);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event == KEY_EVT_SHORT && key == KEY_NEXT) {
        page_mgr_switch(PAGE_BOOKSHELF);
    } else if (event == KEY_EVT_SHORT && key == KEY_HOME) {
        page_mgr_switch(PAGE_SETTINGS);
    }
}

const PageVtbl page_home_vtbl = {
    .id = PAGE_HOME,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

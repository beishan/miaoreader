/**
 * @file page_home.c
 * @brief 主页：日期 + 天气占位 + 阅读统计 + 进入书架
 */
#include "page_home.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "hal/rtc.h"
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

    /* 日期区：y=50 居中 */
    struct tm t;
    char date_str[64] = "-- 周- 1970年01月01日";
    if (ds3231_get_time(&t) == ESP_OK) {
        snprintf(date_str, sizeof(date_str), "%s %d年%02d月%02d日",
                 weekday_cn(t.tm_wday),
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }
    int w = widget_text_width(date_str);
    widget_draw_text((EPD_WIDTH - w) / 2, 50, date_str, EPD_COLOR_BLACK);

    /* 天气占位 y=100 */
    static const char *weather = "晴 25C  PM2.5: 35";
    int ww = widget_text_width(weather);
    widget_draw_text((EPD_WIDTH - ww) / 2, 100, weather, EPD_COLOR_BLACK);

    /* 分割线 y=150 */
    epd_draw_rect(20, 150, EPD_WIDTH - 40, 1, EPD_COLOR_BLACK);

    /* 统计区 y=180..260 */
    widget_draw_text(20, 180, "藏书: -- 册", EPD_COLOR_BLACK);
    widget_draw_text(20, 200, "今日阅读: -- 分钟", EPD_COLOR_BLACK);
    widget_draw_text(20, 220, "正在读: --", EPD_COLOR_BLACK);

    /* 提示：按 NEXT 进入书架 */
    static const char *hint = "[NEXT] 书架";
    int hw = widget_text_width(hint);
    widget_draw_text((EPD_WIDTH - hw) / 2, 270, hint, EPD_COLOR_RED);

    epd_display_full();
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

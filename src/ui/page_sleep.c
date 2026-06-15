/**
 * @file page_sleep.c
 * @brief 休眠页面：时钟天气 / 风景图 / 诗词 三种布局
 */
#include "page_sleep.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "hal/rtc.h"
#include "net/weather.h"
#include "engine/config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

static const char *TAG = "page_sleep";

/* 内置诗词库（简化版，10 首） */
static const char *poems[][3] = {
    {"静夜思", "李白", "床前明月光，疑是地上霜。\n举头望明月，低头思故乡。"},
    {"登鹳雀楼", "王之涣", "白日依山尽，黄河入海流。\n欲穷千里目，更上一层楼。"},
    {"春晓", "孟浩然", "春眠不觉晓，处处闻啼鸟。\n夜来风雨声，花落知多少。"},
    {"相思", "王维", "红豆生南国，春来发几枝。\n愿君多采撷，此物最相思。"},
    {"江雪", "柳宗元", "千山鸟飞绝，万径人踪灭。\n孤舟蓑笠翁，独钓寒江雪。"},
    {"枫桥夜泊", "张继", "月落乌啼霜满天，江枫渔火对愁眠。\n姑苏城外寒山寺，夜半钟声到客船。"},
    {"望庐山瀑布", "李白", "日照香炉生紫烟，遥看瀑布挂前川。\n飞流直下三千尺，疑是银河落九天。"},
    {"绝句", "杜甫", "两个黄鹂鸣翠柳，一行白鹭上青天。\n窗含西岭千秋雪，门泊东吴万里船。"},
    {"游子吟", "孟郊", "慈母手中线，游子身上衣。\n临行密密缝，意恐迟迟归。\n谁言寸草心，报得三春晖。"},
    {"悯农", "李绅", "锄禾日当午，汗滴禾下土。\n谁知盘中餐，粒粒皆辛苦。"},
};

#define POEM_COUNT (sizeof(poems) / sizeof(poems[0]))

static const char *weekday_cn(int wday)
{
    static const char *days[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    if (wday < 0 || wday > 6) return "--";
    return days[wday];
}

/* 布局 0：时钟 + 天气 */
static void render_clock_weather(void)
{
    struct tm t;
    if (ds3231_get_time(&t) != ESP_OK) {
        memset(&t, 0, sizeof(t));
    }

    /* 大字体时间 HH:MM */
    char time_str[8];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t.tm_hour, t.tm_min);
    int tw = widget_text_width(time_str);
    /* 时间居中显示，稍微放大（用两倍绘制模拟大字体） */
    widget_draw_text((EPD_WIDTH - tw) / 2, 60, time_str, EPD_COLOR_BLACK);

    /* 日期 */
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%s %d月%d日",
             weekday_cn(t.tm_wday), t.tm_mon + 1, t.tm_mday);
    int dw = widget_text_width(date_str);
    widget_draw_text((EPD_WIDTH - dw) / 2, 100, date_str, EPD_COLOR_BLACK);

    /* 天气信息 */
    WeatherData weather;
    if (weather_get_current(&weather) == ESP_OK && weather.temp_now != 0) {
        char city_str[80];
        snprintf(city_str, sizeof(city_str), "%s %s", weather.city, weather.condition);
        int cw = widget_text_width(city_str);
        widget_draw_text((EPD_WIDTH - cw) / 2, 140, city_str, EPD_COLOR_BLACK);

        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%d°C", weather.temp_now);
        int ttw = widget_text_width(temp_str);
        widget_draw_text((EPD_WIDTH - ttw) / 2, 160, temp_str, EPD_COLOR_BLACK);
    }
}

/* 布局 1：风景图（简化版：显示默认图案） */
static void render_landscape(void)
{
    /* 绘制简单的山水图案 */
    /* 山峰轮廓 */
    for (int x = 0; x < EPD_WIDTH; x += 2) {
        int h = 150 + (x * 37 % 50) - 25;
        epd_draw_rect(x, h, 1, EPD_HEIGHT - h, EPD_COLOR_BLACK);
    }

    /* 太阳 */
    int sun_x = EPD_WIDTH - 80;
    int sun_y = 50;
    for (int r = 15; r < 20; r++) {
        for (int angle = 0; angle < 360; angle += 10) {
            int px = sun_x + (int)(r * cos(angle * 3.14159 / 180));
            int py = sun_y + (int)(r * sin(angle * 3.14159 / 180));
            if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT) {
                epd_set_pixel(px, py, EPD_COLOR_BLACK);
            }
        }
    }

    /* 底部文字 */
    const char *text = "妙阅 E-Reader";
    int tw = widget_text_width(text);
    widget_draw_text((EPD_WIDTH - tw) / 2, EPD_HEIGHT - 30, text, EPD_COLOR_BLACK);
}

/* 布局 2：诗词 + 时间 */
static void render_poem(void)
{
    struct tm t;
    if (ds3231_get_time(&t) != ESP_OK) {
        memset(&t, 0, sizeof(t));
    }

    /* 随机选择一首诗（基于当前分钟） */
    int idx = t.tm_min % POEM_COUNT;
    const char *title = poems[idx][0];
    const char *author = poems[idx][1];
    const char *content = poems[idx][2];

    /* 诗名 */
    int title_w = widget_text_width(title);
    widget_draw_text((EPD_WIDTH - title_w) / 2, 40, title, EPD_COLOR_BLACK);

    /* 作者 */
    char author_str[32];
    snprintf(author_str, sizeof(author_str), "——%s", author);
    int aw = widget_text_width(author_str);
    widget_draw_text((EPD_WIDTH - aw) / 2, 60, author_str, EPD_COLOR_BLACK);

    /* 诗句（逐行显示） */
    int y = 100;
    const char *line = content;
    while (*line) {
        const char *nl = strchr(line, '\n');
        int len = nl ? (int)(nl - line) : (int)strlen(line);

        char line_buf[64];
        if (len >= (int)sizeof(line_buf)) len = sizeof(line_buf) - 1;
        memcpy(line_buf, line, len);
        line_buf[len] = '\0';

        int lw = widget_text_width(line_buf);
        widget_draw_text((EPD_WIDTH - lw) / 2, y, line_buf, EPD_COLOR_BLACK);

        y += 20;
        line = nl ? nl + 1 : line + len;
    }

    /* 底部时间 */
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02d:%02d  %s",
             t.tm_hour, t.tm_min, weekday_cn(t.tm_wday));
    int tw = widget_text_width(time_str);
    widget_draw_text((EPD_WIDTH - tw) / 2, EPD_HEIGHT - 30, time_str, EPD_COLOR_BLACK);
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入休眠页面");
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);

    /* 根据配置选择布局 */
    SysConfig cfg;
    config_load_sys(&cfg);

    switch (cfg.sleepLayout) {
    case 0:
        render_clock_weather();
        break;
    case 1:
        render_landscape();
        break;
    case 2:
        render_poem();
        break;
    default:
        render_clock_weather();
        break;
    }
}

static void on_key(KeyId key, KeyEvent event)
{
    /* 只响应 PWR 短按唤醒 */
    if (key == KEY_PWR && event == KEY_EVT_SHORT) {
        ESP_LOGI(TAG, "唤醒，返回主页");
        page_mgr_switch(PAGE_HOME);
    }
}

const PageVtbl page_sleep_vtbl = {
    .id = PAGE_SLEEP,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

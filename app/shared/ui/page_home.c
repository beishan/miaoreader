/**
 * @file page_home.c
 * @brief 主页（共享层）
 */
#include "page_home.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include "../mock/mock_rtc.h"
#include "../mock/mock_weather.h"
#include "../mock/mock_books.h"
#include <stdio.h>

static void on_enter(void)
{
    printf("[主页] 进入\n");
}

static void on_render(void)
{
    char buf[128];

    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int y = 40;

    /* 日期区：居中显示 */
    mock_rtc_get_date_str(buf, sizeof(buf));
    int date_w = widget_text_width(buf);
    widget_draw_text((RENDERER_WIDTH - date_w) / 2, y, buf, RENDERER_COLOR_BLACK);

    /* 星期 */
    const char *weekday = mock_rtc_get_weekday_cn();
    int week_w = widget_text_width(weekday);
    widget_draw_text((RENDERER_WIDTH - week_w) / 2, y + 20, weekday, RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(20, y + 45, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 天气区 */
    mock_weather_get_summary(buf, sizeof(buf));
    int weather_w = widget_text_width(buf);
    widget_draw_text((RENDERER_WIDTH - weather_w) / 2, y + 55, buf, RENDERER_COLOR_BLACK);

    mock_weather_get_temp_str(buf, sizeof(buf));
    int temp_w = widget_text_width(buf);
    widget_draw_text((RENDERER_WIDTH - temp_w) / 2, y + 75, buf, RENDERER_COLOR_BLACK);

    mock_weather_get_detail_str(buf, sizeof(buf));
    int detail_w = widget_text_width(buf);
    widget_draw_text((RENDERER_WIDTH - detail_w) / 2, y + 95, buf, RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(20, y + 120, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 统计区：左对齐 */
    mock_books_get_count_str(buf, sizeof(buf));
    widget_draw_text(20, y + 130, buf, RENDERER_COLOR_BLACK);

    mock_books_get_today_reading_str(buf, sizeof(buf));
    widget_draw_text(20, y + 150, buf, RENDERER_COLOR_BLACK);

    mock_books_get_total_reading_str(buf, sizeof(buf));
    widget_draw_text(20, y + 170, buf, RENDERER_COLOR_BLACK);

    /* 分割线 */
    renderer_fill_rect(20, y + 195, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

    /* 当前阅读 */
    mock_books_get_current_book_str(buf, sizeof(buf));
    widget_draw_text(20, y + 205, buf, RENDERER_COLOR_BLACK);

    mock_books_get_progress_str(buf, sizeof(buf));
    widget_draw_text(20, y + 225, buf, RENDERER_COLOR_BLACK);

    /* 进度条 */
    int bar_x = 20, bar_y = y + 250, bar_w = RENDERER_WIDTH - 40;
    renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
    int progress = mock_books_get_progress_percent();
    renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 6, RENDERER_COLOR_BLACK);

    /* 提示 */
    widget_draw_text(120, 270, "[NEXT] 书架", RENDERER_COLOR_RED);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event == KEY_EVT_SHORT) {
        if (key == KEY_NEXT) {
            page_mgr_switch(PAGE_BOOKSHELF);
        } else if (key == KEY_HOME) {
            page_mgr_switch(PAGE_SETTINGS);
        }
    }
}

const PageVtbl page_home_vtbl = {
    .id = PAGE_HOME,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

/**
 * @file page_settings.c
 * @brief 设置页：阅读排版 + 系统设置双层菜单
 */
#include "page_settings.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "engine/config.h"
#include "engine/types.h"
#include "engine/typesetter.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "page_settings";

#define MENU_Y_START  40
#define MENU_LINE_H   22

typedef enum {
    SUB_READING,
    SUB_SYSTEM,
} SettingsSub;

static const char *font_names[]    = {"思源宋体", "思源黑体", "霞鹜文楷"};
static const uint8_t font_sizes[]   = {16, 18, 20, 22, 24, 28, 32};
static const uint8_t line_sp_vals[] = {10, 12, 15, 20};
static const uint8_t char_sp_vals[] = {0, 1, 2, 3, 4};
static const uint8_t margin_vals[]  = {4, 8, 12, 16};

static SettingsSub s_sub = SUB_READING;
static int s_item = 0;
static ReaderConfig s_rc;
static SysConfig s_sc;
static bool s_loaded = false;

static void ensure_loaded(void)
{
    if (s_loaded) return;
    config_load_reader(&s_rc);
    config_load_sys(&s_sc);
    s_loaded = true;
}

static void find_value_index(const uint8_t *arr, int n, uint8_t v, int *idx)
{
    *idx = 0;
    for (int i = 0; i < n; i++) {
        if (arr[i] == v) { *idx = i; return; }
    }
}

/* 找到当前 s_rc.fontSize 在 font_sizes[] 中的索引 */
static int current_size_index(void)
{
    int idx;
    find_value_index(font_sizes, sizeof(font_sizes), s_rc.fontSize, &idx);
    return idx;
}

static void render_reading_menu(void)
{
    static const char *headers[] = {"字体", "字号", "行距", "字间距", "页边距"};
    int font_idx = s_rc.fontId < 3 ? s_rc.fontId : 0;
    int size_idx = current_size_index();
    int line_idx;
    find_value_index(line_sp_vals, sizeof(line_sp_vals), s_rc.lineSpacing, &line_idx);
    int char_idx;
    find_value_index(char_sp_vals, sizeof(char_sp_vals), s_rc.charSpacing, &char_idx);
    int margin_idx;
    find_value_index(margin_vals, sizeof(margin_vals), s_rc.margin, &margin_idx);

    static char values[5][16];
    snprintf(values[0], sizeof(values[0]), "%s", font_names[font_idx]);
    snprintf(values[1], sizeof(values[1]), "%u px", font_sizes[size_idx]);
    snprintf(values[2], sizeof(values[2]), "%.1f", line_sp_vals[line_idx] / 10.0f);
    snprintf(values[3], sizeof(values[3]), "%u px", char_sp_vals[char_idx]);
    snprintf(values[4], sizeof(values[4]), "%u px", margin_vals[margin_idx]);

    widget_draw_text(20, MENU_Y_START - 24, "[阅读排版]", EPD_COLOR_RED);

    for (int i = 0; i < 5; i++) {
        int y = MENU_Y_START + i * MENU_LINE_H;
        char line[40];
        if (i == s_item) {
            snprintf(line, sizeof(line), "> %s", headers[i]);
            widget_draw_text(20, y, line, EPD_COLOR_RED);
        } else {
            snprintf(line, sizeof(line), "  %s", headers[i]);
            widget_draw_text(20, y, line, EPD_COLOR_BLACK);
        }
        /* 右侧值 */
        int vw = widget_text_width(values[i]);
        widget_draw_text(EPD_WIDTH - vw - 20, y, values[i], EPD_COLOR_BLACK);
    }
}

static void render_system_menu(void)
{
    static const char *headers[] = {"休眠布局", "屏幕方向", "休眠超时", "WiFi 状态", "关于"};
    widget_draw_text(20, MENU_Y_START - 24, "[系统设置]", EPD_COLOR_RED);

    static char values[5][32];
    static const char *layouts[] = {"时钟天气", "风景图", "诗词"};
    snprintf(values[0], sizeof(values[0]), "%s", layouts[s_sc.sleepLayout < 3 ? s_sc.sleepLayout : 0]);
    snprintf(values[1], sizeof(values[1]), "%u 度", s_sc.displayRotation * 90);
    if (s_sc.sleepTimeoutSec == 0) {
        snprintf(values[2], sizeof(values[2]), "永不");
    } else {
        snprintf(values[2], sizeof(values[2]), "%lu 分", (unsigned long)s_sc.sleepTimeoutSec / 60);
    }
    snprintf(values[3], sizeof(values[3]), "%s", s_sc.initialized ? "已配网" : "未配网");
    snprintf(values[4], sizeof(values[4]), "v0.2.0");

    for (int i = 0; i < 5; i++) {
        int y = MENU_Y_START + i * MENU_LINE_H;
        char line[40];
        if (i == s_item) {
            snprintf(line, sizeof(line), "> %s", headers[i]);
            widget_draw_text(20, y, line, EPD_COLOR_RED);
        } else {
            snprintf(line, sizeof(line), "  %s", headers[i]);
            widget_draw_text(20, y, line, EPD_COLOR_BLACK);
        }
        int vw = widget_text_width(values[i]);
        widget_draw_text(EPD_WIDTH - vw - 20, y, values[i], EPD_COLOR_BLACK);
    }
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入设置");
    ensure_loaded();
    s_sub = SUB_READING;
    s_item = 0;
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();
    if (s_sub == SUB_READING) render_reading_menu();
    else                       render_system_menu();

    /* 底部提示 */
    widget_draw_text(20, EPD_HEIGHT - 14, "[PREV/NEXT]选 [HOME]切换值 [PWR]返回", EPD_COLOR_BLACK);

    epd_display_full();
}

static void cycle_reading(int delta)
{
    switch (s_item) {
    case 0: /* font */
        s_rc.fontId = (s_rc.fontId + delta + 3) % 3;
        break;
    case 1: /* size */
    {
        int idx = current_size_index();
        idx = (idx + delta + (int)(sizeof(font_sizes) / sizeof(font_sizes[0])))
              % (int)(sizeof(font_sizes) / sizeof(font_sizes[0]));
        s_rc.fontSize = font_sizes[idx];
        break;
    }
    case 2: /* line spacing */
    {
        int n = sizeof(line_sp_vals) / sizeof(line_sp_vals[0]);
        int idx;
        find_value_index(line_sp_vals, n, s_rc.lineSpacing, &idx);
        idx = (idx + delta + n) % n;
        s_rc.lineSpacing = line_sp_vals[idx];
        break;
    }
    case 3: /* char spacing */
    {
        int n = sizeof(char_sp_vals) / sizeof(char_sp_vals[0]);
        int idx;
        find_value_index(char_sp_vals, n, s_rc.charSpacing, &idx);
        idx = (idx + delta + n) % n;
        s_rc.charSpacing = char_sp_vals[idx];
        break;
    }
    case 4: /* margin */
    {
        int n = sizeof(margin_vals) / sizeof(margin_vals[0]);
        int idx;
        find_value_index(margin_vals, n, s_rc.margin, &idx);
        idx = (idx + delta + n) % n;
        s_rc.margin = margin_vals[idx];
        break;
    }
    }
    config_save_reader(&s_rc);
    TypesetterConfig tc = {
        .fontId = s_rc.fontId, .fontSize = s_rc.fontSize,
        .lineSpacing = s_rc.lineSpacing, .charSpacing = s_rc.charSpacing,
        .margin = s_rc.margin, .pageWidth = EPD_WIDTH - s_rc.margin * 2,
        .pageHeight = 262 - s_rc.margin * 2,
    };
    typesetter_reload_config(&tc);
}

static void on_key(KeyId key, KeyEvent event)
{
    /* 长按切换值（阅读排版设置） */
    if (event == KEY_EVT_LONG && s_sub == SUB_READING) {
        if (key == KEY_PREV) {
            cycle_reading(-1);
            on_render();
        } else if (key == KEY_NEXT) {
            cycle_reading(+1);
            on_render();
        }
        return;
    }

    /* 短按事件 */
    if (event != KEY_EVT_SHORT) return;

    if (key == KEY_PREV) {
        if (s_item > 0) s_item--;
        else if (s_sub == SUB_READING) { s_sub = SUB_SYSTEM; s_item = 4; }
        on_render();
    } else if (key == KEY_NEXT) {
        int max_item = 4;
        if (s_item < max_item) s_item++;
        else if (s_sub == SUB_SYSTEM) { s_sub = SUB_READING; s_item = 0; }
        on_render();
    } else if (key == KEY_HOME) {
        /* 切换子菜单 */
        if (s_sub == SUB_READING) s_sub = SUB_SYSTEM;
        else                       s_sub = SUB_READING;
        s_item = 0;
        on_render();
    } else if (key == KEY_PWR) {
        page_mgr_switch(PAGE_HOME);
    }
}

const PageVtbl page_settings_vtbl = {
    .id = PAGE_SETTINGS,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

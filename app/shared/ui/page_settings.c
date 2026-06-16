/**
 * @file page_settings.c
 * @brief 设置页交互版（共享层）
 *
 * BROWSE/EDIT 双模式状态机：
 *   BROWSE: PREV/NEXT 移动光标，HOME 进入编辑，PWR 返回主页
 *   EDIT:   PREV/NEXT 切换值，HOME 确认，PWR 取消
 */
#include "page_settings.h"
#include "widget.h"
#include "status_bar.h"
#include "renderer_if.h"
#include <stdio.h>
#include <string.h>

/* ---- 数据结构 ---- */

typedef struct {
    const char *label;
    const char *options[8];
    int option_count;
    int current_value;
    int edit_value;
} SettingItem;

/* ---- 设置项定义 ---- */

static SettingItem s_settings[] = {
    { "Size",         { "16px", "18px", "20px", "22px", "24px", "28px", "32px" }, 7, 2, 2 },
    { "Line Spacing", { "1.0",  "1.2",  "1.5",  "2.0" },                          4, 2, 2 },
    { "Char Spacing", { "0px",  "1px",  "2px",  "3px",  "4px" },                  5, 1, 1 },
    { "Margin",       { "4px",  "8px",  "12px", "16px" },                          4, 1, 1 },
    { "Auto Sleep",   { "5min", "10min", "30min", "Off" },                         4, 0, 0 },
};

#define SETTING_COUNT  (sizeof(s_settings) / sizeof(s_settings[0]))

/* 分界线索引：前 4 项为阅读排版，第 5 项为系统设置 */
#define READING_COUNT  4

/* ---- 状态 ---- */

typedef enum {
    MODE_BROWSE,
    MODE_EDIT,
} SettingsMode;

static SettingsMode s_mode;
static int s_cursor;          /* 当前光标位置 0..SETTING_COUNT-1 */

/* ---- 布局常量 ---- */

#define TITLE_Y       30
#define SECTION1_Y    55
#define ITEM_H        18
#define SEPARATOR_Y   (SECTION1_Y + READING_COUNT * ITEM_H + 5)
#define SECTION2_Y    (SEPARATOR_Y + 12)
#define HINT_Y        (RENDERER_HEIGHT - 20)
#define LEFT_X        20
#define VALUE_X       200

/* ---- 回调 ---- */

static void on_enter(void)
{
    s_mode = MODE_BROWSE;
    s_cursor = 0;
    /* 恢复 current_value 到 edit_value */
    for (int i = 0; i < (int)SETTING_COUNT; i++) {
        s_settings[i].edit_value = s_settings[i].current_value;
    }
    printf("[设置] 进入\n");
}

static void render_section_title(int y, const char *title)
{
    widget_draw_text(LEFT_X, y, title, RENDERER_COLOR_RED);
}

static void render_item(int y, int index)
{
    SettingItem *s = &s_settings[index];
    int is_cursor = (s_mode == MODE_BROWSE && s_cursor == index);
    int is_editing = (s_mode == MODE_EDIT && s_cursor == index);

    /* 光标指示 */
    if (is_cursor || is_editing) {
        widget_draw_text(LEFT_X, y, "▶", RENDERER_COLOR_BLACK);
    }

    /* 标签 */
    int label_x = LEFT_X + 14;
    widget_draw_text(label_x, y, s->label, RENDERER_COLOR_BLACK);

    /* 值 */
    const char *val_str;
    RendererColor val_color;
    if (is_editing) {
        val_str = s->options[s->edit_value];
        val_color = RENDERER_COLOR_RED;
    } else {
        val_str = s->options[s->current_value];
        val_color = RENDERER_COLOR_BLACK;
    }
    widget_draw_text(VALUE_X, y, val_str, val_color);
}

static void on_render(void)
{
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    /* 标题 */
    widget_draw_text(LEFT_X, TITLE_Y, "Settings", RENDERER_COLOR_BLACK);
    renderer_fill_rect(LEFT_X, TITLE_Y + 18, RENDERER_WIDTH - 2 * LEFT_X, 1, RENDERER_COLOR_BLACK);

    /* 阅读排版 */
    render_section_title(SECTION1_Y, "[阅读排版]");
    for (int i = 0; i < READING_COUNT; i++) {
        int y = SECTION1_Y + 18 + i * ITEM_H;
        render_item(y, i);
    }

    /* 分隔线 */
    renderer_fill_rect(LEFT_X, SEPARATOR_Y, RENDERER_WIDTH - 2 * LEFT_X, 1, RENDERER_COLOR_BLACK);

    /* 系统设置 */
    render_section_title(SECTION2_Y, "[系统设置]");
    for (int i = READING_COUNT; i < (int)SETTING_COUNT; i++) {
        int y = SECTION2_Y + 18 + (i - READING_COUNT) * ITEM_H;
        render_item(y, i);
    }

    /* 底部提示 */
    if (s_mode == MODE_BROWSE) {
        widget_draw_text(LEFT_X, HINT_Y, "PREV/NEXT Move  HOME Edit  PWR Back", RENDERER_COLOR_BLACK);
    } else {
        widget_draw_text(LEFT_X, HINT_Y, "PREV/NEXT Change  HOME OK  PWR Cancel", RENDERER_COLOR_BLACK);
    }
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    if (s_mode == MODE_BROWSE) {
        switch (key) {
        case KEY_PREV:
            if (s_cursor > 0) s_cursor--;
            break;
        case KEY_NEXT:
            if (s_cursor < (int)SETTING_COUNT - 1) s_cursor++;
            break;
        case KEY_HOME:
            s_mode = MODE_EDIT;
            s_settings[s_cursor].edit_value = s_settings[s_cursor].current_value;
            break;
        case KEY_PWR:
            page_mgr_switch(PAGE_HOME);
            break;
        default:
            break;
        }
    } else { /* MODE_EDIT */
        switch (key) {
        case KEY_PREV: {
            int *v = &s_settings[s_cursor].edit_value;
            *v = (*v - 1 + s_settings[s_cursor].option_count) % s_settings[s_cursor].option_count;
            break;
        }
        case KEY_NEXT: {
            int *v = &s_settings[s_cursor].edit_value;
            *v = (*v + 1) % s_settings[s_cursor].option_count;
            break;
        }
        case KEY_HOME:
            /* 确认 */
            s_settings[s_cursor].current_value = s_settings[s_cursor].edit_value;
            s_mode = MODE_BROWSE;
            printf("[设置] %s -> %s\n",
                   s_settings[s_cursor].label,
                   s_settings[s_cursor].options[s_settings[s_cursor].current_value]);
            break;
        case KEY_PWR:
            /* 取消，恢复 */
            s_settings[s_cursor].edit_value = s_settings[s_cursor].current_value;
            s_mode = MODE_BROWSE;
            break;
        default:
            break;
        }
    }
}

const PageVtbl page_settings_vtbl = {
    .id = PAGE_SETTINGS,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

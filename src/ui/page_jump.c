/**
 * @file page_jump.c
 * @brief 跳转页码界面：数字输入
 */
#include "page_jump.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "page_jump";

/* 页码信息 */
static uint32_t s_current_page = 0;
static uint32_t s_total_pages = 0;

/* 输入的页码（各位数字） */
static int s_digits[4] = {0};  /* 最多 4 位数 */
static int s_digit_count = 0;
static int s_cursor = 0;  /* 当前光标位置 */

void page_jump_set_page_info(uint32_t current_page, uint32_t total_pages)
{
    s_current_page = current_page;
    s_total_pages = total_pages;
    s_digit_count = 0;
    s_cursor = 0;
    memset(s_digits, 0, sizeof(s_digits));
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入跳转界面");
    s_digit_count = 0;
    s_cursor = 0;
    memset(s_digits, 0, sizeof(s_digits));
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    /* 标题 */
    const char *title = "跳转到页码";
    int tw = widget_text_width(title);
    widget_draw_text((EPD_WIDTH - tw) / 2, 40, title, EPD_COLOR_BLACK);

    /* 分割线 */
    epd_draw_rect(40, 60, EPD_WIDTH - 80, 1, EPD_COLOR_BLACK);

    /* 当前输入的页码 */
    char page_str[16] = {0};
    int pos = 0;
    for (int i = 0; i < s_digit_count; i++) {
        page_str[pos++] = '0' + s_digits[i];
    }
    if (s_cursor < s_digit_count) {
        page_str[pos++] = '_';
    } else {
        page_str[pos++] = '_';
    }
    page_str[pos] = '\0';

    int pw = widget_text_width(page_str);
    widget_draw_text((EPD_WIDTH - pw) / 2, 100, page_str, EPD_COLOR_BLACK);

    /* 范围提示 */
    char range_str[32];
    snprintf(range_str, sizeof(range_str), "范围: 1 - %lu", (unsigned long)s_total_pages);
    int rw = widget_text_width(range_str);
    widget_draw_text((EPD_WIDTH - rw) / 2, 130, range_str, EPD_COLOR_BLACK);

    /* 当前页码 */
    char current_str[32];
    snprintf(current_str, sizeof(current_str), "当前: 第 %lu 页", (unsigned long)(s_current_page + 1));
    int cw = widget_text_width(current_str);
    widget_draw_text((EPD_WIDTH - cw) / 2, 160, current_str, EPD_COLOR_BLACK);

    /* 操作提示 */
    widget_draw_text(20, 200, "PREV/NEXT: 调整数字", EPD_COLOR_BLACK);
    widget_draw_text(20, 220, "HOME: 确认跳转", EPD_COLOR_BLACK);
    widget_draw_text(20, 240, "PWR: 取消返回", EPD_COLOR_BLACK);
}

static void on_key(KeyId key, KeyEvent event)
{
    if (event != KEY_EVT_SHORT) return;

    switch (key) {
    case KEY_PREV:
        /* 减少当前位数字 */
        if (s_digit_count > 0) {
            s_digits[s_digit_count - 1]--;
            if (s_digits[s_digit_count - 1] < 0) {
                s_digits[s_digit_count - 1] = 9;
            }
        }
        on_render();
        break;

    case KEY_NEXT:
        /* 增加当前位数字或添加新位 */
        if (s_digit_count < 4) {
            if (s_digit_count == 0) {
                s_digits[0] = 1;
                s_digit_count = 1;
            } else {
                s_digits[s_digit_count - 1]++;
                if (s_digits[s_digit_count - 1] > 9) {
                    s_digits[s_digit_count - 1] = 0;
                    /* 进位 */
                    if (s_digit_count < 4) {
                        s_digits[s_digit_count] = 0;
                        s_digit_count++;
                    }
                }
            }
        }
        on_render();
        break;

    case KEY_HOME:
        /* 确认跳转 */
        if (s_digit_count > 0) {
            /* 计算页码 */
            uint32_t target = 0;
            for (int i = 0; i < s_digit_count; i++) {
                target = target * 10 + s_digits[i];
            }

            /* 范围检查 */
            if (target < 1) target = 1;
            if (target > s_total_pages) target = s_total_pages;

            ESP_LOGI(TAG, "跳转到第 %lu 页", (unsigned long)target);

            /* 返回菜单，菜单会返回阅读器并设置目标页 */
            /* 通过全局变量传递目标页码 */
            page_jump_set_page_info(target - 1, s_total_pages);
            page_mgr_pop();
            page_mgr_pop();  /* 菜单 -> 阅读器 */
        }
        break;

    case KEY_PWR:
        /* 取消返回 */
        page_mgr_pop();
        break;

    default:
        break;
    }
}

const PageVtbl page_jump_vtbl = {
    .id = PAGE_JUMP,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

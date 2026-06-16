#ifdef PC_SIMULATION

#include "page_jump.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "status_bar.h"
#include <stdio.h>
#include <string.h>

static uint32_t s_current_page = 0;
static uint32_t s_total_pages = 0;
static int s_digits[4] = {0};
static int s_digit_count = 0;

void page_jump_set_page_info(uint32_t current_page, uint32_t total_pages) {
    s_current_page = current_page;
    s_total_pages = total_pages;
    memset(s_digits, 0, sizeof(s_digits));
    s_digit_count = 0;
}

static void on_enter(void) {
    memset(s_digits, 0, sizeof(s_digits));
    s_digit_count = 0;
}

static uint32_t get_target_page(void) {
    uint32_t target = 0;
    for (int i = 0; i < s_digit_count; i++) {
        target = target * 10 + s_digits[i];
    }
    return target;
}

static void on_render(void);

static void on_key(KeyId key, KeyEvent event) {
    if (event != KEY_EVT_SHORT) return;

    switch (key) {
        case KEY_PREV:
            if (s_digit_count > 0) {
                s_digits[s_digit_count - 1]--;
                if (s_digits[s_digit_count - 1] < 0) {
                    s_digits[s_digit_count - 1] = 9;
                }
            }
            break;
        case KEY_NEXT:
            if (s_digit_count == 0) {
                s_digits[0] = 1;
                s_digit_count = 1;
            } else {
                s_digits[s_digit_count - 1]++;
                if (s_digits[s_digit_count - 1] > 9) {
                    s_digits[s_digit_count - 1] = 0;
                    if (s_digit_count < 4) {
                        s_digit_count++;
                        s_digits[s_digit_count - 1] = 1;
                    }
                }
            }
            break;
        case KEY_HOME: {
            uint32_t target = get_target_page();
            if (target < 1) target = 1;
            if (target > s_total_pages) target = s_total_pages;
            // 返回阅读器（pop 两次：jump -> menu -> reader）
            page_mgr_pop();
            page_mgr_pop();
            break;
        }
        case KEY_PWR:
            page_mgr_pop();
            break;
        default:
            break;
    }

    // 触发重绘
    on_render();
}

static void on_render(void) {
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int cx = RENDERER_WIDTH / 2;

    // 标题
    const char *title = "跳转到页码";
    int tw = renderer_text_width(title);
    renderer_draw_text(cx - tw / 2, 40, title, RENDERER_COLOR_BLACK);

    // 分割线
    renderer_draw_rect(40, 60, RENDERER_WIDTH - 80, 1, RENDERER_COLOR_BLACK);

    // 输入显示
    char input_buf[16] = {0};
    for (int i = 0; i < s_digit_count; i++) {
        input_buf[i] = '0' + s_digits[i];
    }
    input_buf[s_digit_count] = '_';
    input_buf[s_digit_count + 1] = '\0';
    int iw = renderer_text_width(input_buf);
    renderer_draw_text(cx - iw / 2, 100, input_buf, RENDERER_COLOR_BLACK);

    // 范围提示
    char range_buf[32];
    snprintf(range_buf, sizeof(range_buf), "范围: 1 - %u", s_total_pages);
    int rw = renderer_text_width(range_buf);
    renderer_draw_text(cx - rw / 2, 130, range_buf, RENDERER_COLOR_BLACK);

    // 当前页码
    char curr_buf[32];
    snprintf(curr_buf, sizeof(curr_buf), "当前: 第 %u 页", s_current_page + 1);
    int cw = renderer_text_width(curr_buf);
    renderer_draw_text(cx - cw / 2, 160, curr_buf, RENDERER_COLOR_BLACK);

    // 操作提示
    const char *hints[] = {
        "PREV/NEXT: 调整数字",
        "HOME: 确认跳转",
        "PWR: 取消返回",
    };
    int y = 200;
    for (int i = 0; i < 3; i++) {
        int hw = renderer_text_width(hints[i]);
        renderer_draw_text(cx - hw / 2, y, hints[i], RENDERER_COLOR_BLACK);
        y += 20;
    }

    renderer_display();
}

const PageVtbl page_jump_vtbl = {
    .id = PAGE_JUMP,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#endif // PC_SIMULATION

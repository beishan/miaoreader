/**
 * @file page_reader.c
 * @brief 阅读界面：加载书籍 → 排版分页 → 渲染 + 翻页（局部/全刷交替）
 */
#include "page_reader.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "ui/page_menu.h"
#include "engine/types.h"
#include "engine/config.h"
#include "engine/typesetter.h"
#include "engine/book_parser.h"
#include "engine/book_meta.h"
#include "engine/reading_stats.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "page_reader";

#define PARTIAL_CLEAN_INTERVAL 5
#define CONTENT_Y 24    /* 状态栏 20 + 4 间距 */
#define CONTENT_H 256   /* 300 - 20 - 24 */
#define PROGRESS_Y 286

static char    *s_book_text = NULL;
static uint32_t s_text_len = 0;
static PageIndex *s_pages = NULL;
static uint32_t s_page_count = 0;
static uint32_t s_current_page = 0;
static int      s_partial_count = 0;
static char     s_current_path[256] = {0};
static int      s_page_turn_count = 0;  /* 翻页计数，用于定期保存 */

static void free_book(void)
{
    if (s_book_text) { book_free_text(s_book_text); s_book_text = NULL; }
    if (s_pages)     { typesetter_free_pages(s_pages); s_pages = NULL; }
    s_page_count = 0;
    s_text_len = 0;
    s_current_page = 0;
}

/* 保存阅读进度到 NVS */
static void save_reading_progress(void)
{
    if (s_current_path[0] == '\0' || s_current_page == 0) return;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader_progress", NVS_READWRITE, &handle);
    if (err != ESP_OK) return;

    /* 使用书名作为 key（简化：用文件路径的 hash） */
    char key[16];
    uint32_t h = 2166136261u;
    for (const char *p = s_current_path; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    snprintf(key, sizeof(key), "pg_%08lx", (unsigned long)h);

    nvs_set_u32(handle, key, s_current_page);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "保存阅读进度: %s -> 第 %lu 页", key, (unsigned long)s_current_page);
}

/* 从 NVS 加载阅读进度 */
static uint32_t load_reading_progress(const char *file_path)
{
    if (!file_path || file_path[0] == '\0') return 0;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader_progress", NVS_READONLY, &handle);
    if (err != ESP_OK) return 0;

    char key[16];
    uint32_t h = 2166136261u;
    for (const char *p = file_path; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    snprintf(key, sizeof(key), "pg_%08lx", (unsigned long)h);

    uint32_t page = 0;
    nvs_get_u32(handle, key, &page);
    nvs_close(handle);

    if (page > 0) {
        ESP_LOGI(TAG, "恢复阅读进度: %s -> 第 %lu 页", key, (unsigned long)page);
    }
    return page;
}

/* 保存当前书籍路径到 NVS（用于崩溃恢复） */
static void save_current_book_path(void)
{
    if (s_current_path[0] == '\0') return;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader_progress", NVS_READWRITE, &handle);
    if (err != ESP_OK) return;

    nvs_set_str(handle, "last_book", s_current_path);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "保存当前书籍: %s", s_current_path);
}

/* 从 NVS 加载上次阅读的书籍路径 */
static bool load_last_book_path(char *buf, size_t buf_size)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader_progress", NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    size_t len = buf_size;
    err = nvs_get_str(handle, "last_book", buf, &len);
    nvs_close(handle);

    return (err == ESP_OK && buf[0] != '\0');
}

/* 定期保存进度（每 5 次翻页保存一次） */
static void periodic_save_progress(void)
{
    s_page_turn_count++;
    if (s_page_turn_count >= 5) {
        save_reading_progress();
        s_page_turn_count = 0;
    }
}

/* 设置当前书籍路径（供书架调用） */
void page_reader_set_book(const char *file_path)
{
    if (file_path) {
        strncpy(s_current_path, file_path, sizeof(s_current_path) - 1);
        s_current_path[sizeof(s_current_path) - 1] = '\0';
    }
}

/* 尝试从 NVS 恢复上次阅读（供 main.c 启动时调用） */
bool page_reader_try_restore(void)
{
    char path[256];
    if (!load_last_book_path(path, sizeof(path))) {
        return false;
    }

    /* 检查文件是否存在 */
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "上次书籍文件不存在: %s", path);
        return false;
    }
    fclose(f);

    ESP_LOGI(TAG, "发现上次阅读记录: %s", path);
    strncpy(s_current_path, path, sizeof(s_current_path) - 1);
    s_current_path[sizeof(s_current_path) - 1] = '\0';
    return true;
}

static esp_err_t load_book(const char *file_path)
{
    free_book();
    strncpy(s_current_path, file_path, sizeof(s_current_path) - 1);

    esp_err_t err = book_load_text(file_path, &s_book_text, &s_text_len);
    if (err != ESP_OK || !s_book_text) {
        ESP_LOGE(TAG, "加载书籍失败: %s (0x%x)", file_path, err);
        return err;
    }

    ReaderConfig rc;
    config_load_reader(&rc);
    TypesetterConfig tc = {
        .fontId = rc.fontId, .fontSize = rc.fontSize,
        .lineSpacing = rc.lineSpacing, .charSpacing = rc.charSpacing,
        .margin = rc.margin,
        .pageWidth = EPD_WIDTH - rc.margin * 2,
        .pageHeight = CONTENT_H - rc.margin * 2,
    };
    err = typesetter_init(&tc);
    if (err == ESP_OK) {
        err = typesetter_paginate(s_book_text, s_text_len, &s_pages, &s_page_count);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "分页失败: 0x%x", err);
        free_book();
        return err;
    }
    ESP_LOGI(TAG, "书籍已加载: %u 字节 → %u 页", (unsigned)s_text_len, (unsigned)s_page_count);
    return ESP_OK;
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入阅读器");
    s_partial_count = 0;
    s_page_turn_count = 0;

    /* 开始阅读计时 */
    reading_stats_start_session();

    /* 如果有书籍路径，尝试恢复阅读进度 */
    if (s_current_path[0] != '\0' && s_book_text) {
        uint32_t saved_page = load_reading_progress(s_current_path);
        if (saved_page > 0 && saved_page < s_page_count) {
            s_current_page = saved_page;
            ESP_LOGI(TAG, "恢复到第 %lu 页", (unsigned long)s_current_page);
        }
        /* 保存当前书籍路径到 NVS（用于崩溃恢复） */
        save_current_book_path();
        return;
    }

    /* 如果有路径但未加载书籍（崩溃恢复场景） */
    if (s_current_path[0] != '\0' && !s_book_text) {
        if (load_book(s_current_path) == ESP_OK) {
            uint32_t saved_page = load_reading_progress(s_current_path);
            if (saved_page > 0 && saved_page < s_page_count) {
                s_current_page = saved_page;
            }
            save_current_book_path();
            return;
        }
        ESP_LOGW(TAG, "崩溃恢复失败，无法加载: %s", s_current_path);
        s_current_path[0] = '\0';
    }

    /* 演示：从占位路径加载（生产环境由 page_bookshelf 注入当前书路径） */
    if (!s_book_text && s_current_path[0] == '\0') {
        const char *demo = "/sdcard/books/placeholder_1.txt";
        if (load_book(demo) != ESP_OK) {
            /* 失败时填入示例文本 */
            s_book_text = strdup("示例文本\n\n这是一本占位书籍。\n\n按 NEXT 翻页。\n\n按 PREV 返回。\n\n按 HOME 返回书架。");
            s_text_len = strlen(s_book_text);
            s_current_page = 0;
            s_page_count = 1;  /* 占位不分页 */
        }
    }
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    if (s_book_text && s_pages && s_current_page < s_page_count) {
        /* 渲染当前页到 EPD（直接走 epd_set_pixel 接口） */
        typesetter_render_page(s_book_text, &s_pages[s_current_page],
                                NULL, EPD_WIDTH, EPD_HEIGHT, CONTENT_Y);
    } else if (s_book_text) {
        /* 占位模式：用 widget 字体显示示例文本 */
        widget_draw_text(20, 40, s_book_text, EPD_COLOR_BLACK);
    } else {
        widget_draw_text(20, 100, "未加载书籍", EPD_COLOR_RED);
    }

    /* 底部页码（无进度条） */
    char info[32];
    snprintf(info, sizeof(info), "< %u/%u >", (unsigned)(s_current_page + 1), (unsigned)s_page_count);
    int iw = widget_text_width(info);
    widget_draw_text((EPD_WIDTH - iw) / 2, EPD_HEIGHT - 18, info, EPD_COLOR_BLACK);

    /* 局部/全刷交替：每 5 次全刷一次 */
    if (s_partial_count >= PARTIAL_CLEAN_INTERVAL) {
        epd_display_full();
        s_partial_count = 0;
    } else {
        epd_display_partial();
        s_partial_count++;
    }
}

static void page_reader_on_exit(void)
{
    ESP_LOGI(TAG, "退出阅读器");

    /* 保存阅读进度到 NVS */
    save_reading_progress();

    /* 更新 book_meta 中的进度 */
    if (s_current_path[0] != '\0') {
        book_meta_update_page(s_current_path, s_current_page);
        book_meta_update_total_pages(s_current_path, s_page_count);
    }

    /* 结束阅读计时 */
    reading_stats_end_session();
}

static void on_key(KeyId key, KeyEvent event)
{
    if (key == KEY_PREV) {
        if (event == KEY_EVT_SHORT && s_current_page > 0) {
            s_current_page--;
            periodic_save_progress();
            on_render();
        } else if (event == KEY_EVT_LONG && s_current_page >= 5) {
            s_current_page -= 5;
            periodic_save_progress();
            on_render();
        }
    } else if (key == KEY_NEXT) {
        if (event == KEY_EVT_SHORT && s_current_page + 1 < s_page_count) {
            s_current_page++;
            periodic_save_progress();
            on_render();
        } else if (event == KEY_EVT_LONG && s_current_page + 5 < s_page_count) {
            s_current_page += 5;
            periodic_save_progress();
            on_render();
        }
    } else if (key == KEY_HOME && event == KEY_EVT_SHORT) {
        /* 弹出阅读上下文菜单 */
        const char *book_name = strrchr(s_current_path, '/');
        book_name = book_name ? book_name + 1 : s_current_path;
        page_menu_set_book_info(book_name, s_current_page, s_page_count);
        page_mgr_push(PAGE_MENU);
    }
}

const PageVtbl page_reader_vtbl = {
    .id = PAGE_READER,
    .on_enter = on_enter,
    .on_exit = page_reader_on_exit,
    .on_key = on_key,
    .on_render = on_render,
};

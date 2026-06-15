/**
 * @file page_mgr.c
 * @brief 页面状态机：注册/切换/栈式导航/按键分发
 */
#include "page_mgr.h"
#include "hal/epd.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "page_mgr";

#define MAX_PAGES 8
#define MAX_STACK 4

static PageVtbl pages[MAX_PAGES];
static int page_count = 0;
static PageId current_page = PAGE_BOOT;

/* 页面栈（用于 push/pop 导航） */
static PageId page_stack[MAX_STACK];
static int stack_top = 0;

esp_err_t page_mgr_init(void)
{
    ESP_LOGI(TAG, "初始化页面状态机");
    memset(pages, 0, sizeof(pages));
    page_count = 0;
    current_page = PAGE_BOOT;
    stack_top = 0;
    return ESP_OK;
}

esp_err_t page_mgr_register(const PageVtbl *page)
{
    if (!page || page->id >= MAX_PAGES) {
        return ESP_ERR_INVALID_ARG;
    }

    pages[page->id] = *page;
    page_count++;
    ESP_LOGI(TAG, "注册页面 %d", page->id);

    return ESP_OK;
}

/* 内部：执行页面切换 */
static esp_err_t do_switch(PageId id)
{
    if (id >= MAX_PAGES) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!pages[id].on_enter) {
        ESP_LOGE(TAG, "页面 %d 未注册", id);
        return ESP_ERR_NOT_FOUND;
    }

    if (pages[current_page].on_exit) {
        pages[current_page].on_exit();
    }

    current_page = id;

    if (pages[current_page].on_enter) {
        pages[current_page].on_enter();
    }

    if (pages[current_page].on_render) {
        pages[current_page].on_render();
    }

    epd_display_full();

    ESP_LOGI(TAG, "切换到页面 %d", current_page);
    return ESP_OK;
}

esp_err_t page_mgr_switch(PageId id)
{
    return do_switch(id);
}

esp_err_t page_mgr_push(PageId id)
{
    if (stack_top >= MAX_STACK) {
        ESP_LOGW(TAG, "页面栈已满，无法 push");
        return ESP_ERR_NO_MEM;
    }

    /* 将当前页面压栈 */
    page_stack[stack_top] = current_page;
    stack_top++;
    ESP_LOGI(TAG, "push: %d -> %d (栈深度: %d)", current_page, id, stack_top);

    return do_switch(id);
}

esp_err_t page_mgr_pop(void)
{
    if (stack_top <= 0) {
        ESP_LOGW(TAG, "页面栈为空，无法 pop");
        return ESP_ERR_INVALID_STATE;
    }

    stack_top--;
    PageId prev = page_stack[stack_top];
    ESP_LOGI(TAG, "pop: %d -> %d (栈深度: %d)", current_page, prev, stack_top);

    return do_switch(prev);
}

PageId page_mgr_current(void)
{
    return current_page;
}

void page_mgr_handle_key(KeyId key, KeyEvent event)
{
    if (pages[current_page].on_key) {
        pages[current_page].on_key(key, event);
    }
}

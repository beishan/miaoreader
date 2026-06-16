/**
 * @file page_mgr.c
 * @brief 页面状态机（共享层）
 */
#include "page_mgr.h"
#include "renderer_if.h"
#include <string.h>
#include <stdio.h>

#define MAX_PAGES 10
#define MAX_STACK 4

static PageVtbl pages[MAX_PAGES];
static int page_count = 0;
static PageId current_page = PAGE_BOOT;

static PageId page_stack[MAX_STACK];
static int stack_top = 0;

esp_err_t page_mgr_init(void)
{
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
    return ESP_OK;
}

static esp_err_t do_switch(PageId id)
{
    if (id >= MAX_PAGES) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!pages[id].on_enter) {
        printf("[page_mgr] 页面 %d 未注册\n", id);
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

    renderer_display();

    printf("[page_mgr] 切换到页面 %d\n", current_page);
    return ESP_OK;
}

esp_err_t page_mgr_switch(PageId id)
{
    return do_switch(id);
}

esp_err_t page_mgr_push(PageId id)
{
    if (stack_top >= MAX_STACK) {
        printf("[page_mgr] 页面栈已满\n");
        return ESP_ERR_NO_MEM;
    }

    page_stack[stack_top] = current_page;
    stack_top++;
    printf("[page_mgr] push: %d -> %d (栈深度: %d)\n", current_page, id, stack_top);

    return do_switch(id);
}

esp_err_t page_mgr_pop(void)
{
    if (stack_top <= 0) {
        printf("[page_mgr] 页面栈为空\n");
        return ESP_ERR_INVALID_STATE;
    }

    stack_top--;
    PageId prev = page_stack[stack_top];
    printf("[page_mgr] pop: %d -> %d (栈深度: %d)\n", current_page, prev, stack_top);

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

const PageVtbl *page_mgr_get_current_vtbl(void)
{
    if (current_page >= MAX_PAGES || !pages[current_page].on_enter) {
        return NULL;
    }
    return &pages[current_page];
}

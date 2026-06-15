#include "page_mgr.h"
#include "hal/epd.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "page_mgr";

#define MAX_PAGES 6

static PageVtbl pages[MAX_PAGES];
static int page_count = 0;
static PageId current_page = PAGE_BOOT;

esp_err_t page_mgr_init(void)
{
    ESP_LOGI(TAG, "初始化页面状态机");
    memset(pages, 0, sizeof(pages));
    page_count = 0;
    current_page = PAGE_BOOT;
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

esp_err_t page_mgr_switch(PageId id)
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

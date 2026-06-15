#pragma once
#include "engine/types.h"
#include "esp_err.h"

typedef struct {
    PageId id;
    void (*on_enter)(void);
    void (*on_exit)(void);
    void (*on_key)(KeyId key, KeyEvent event);
    void (*on_render)(void);
} PageVtbl;

esp_err_t page_mgr_init(void);
esp_err_t page_mgr_register(const PageVtbl *page);
esp_err_t page_mgr_switch(PageId id);
PageId page_mgr_current(void);
void page_mgr_handle_key(KeyId key, KeyEvent event);

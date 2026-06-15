/**
 * @file page_mgr.h
 * @brief 页面状态机：注册/切换/栈式导航/按键分发
 */
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

/* 扁平切换（无历史记录） */
esp_err_t page_mgr_switch(PageId id);

/* 栈式导航（支持返回） */
esp_err_t page_mgr_push(PageId id);
esp_err_t page_mgr_pop(void);

PageId page_mgr_current(void);
void page_mgr_handle_key(KeyId key, KeyEvent event);

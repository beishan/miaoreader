/**
 * @file page_mgr.h
 * @brief 页面状态机（共享层）
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 页面 ID */
typedef enum {
    PAGE_BOOT,
    PAGE_HOME,
    PAGE_BOOKSHELF,
    PAGE_READER,
    PAGE_SETTINGS,
    PAGE_SLEEP,
    PAGE_MENU,
    PAGE_JUMP,
} PageId;

/* 按键事件 */
typedef enum {
    KEY_EVT_SHORT,
    KEY_EVT_LONG,
    KEY_EVT_SUPER_LONG,
    KEY_EVT_COMBO,
} KeyEvent;

/* 按键 ID */
typedef enum {
    KEY_PWR,
    KEY_PREV,
    KEY_NEXT,
    KEY_HOME,
    KEY_COUNT,
} KeyId;

/* 页面虚函数表 */
typedef struct {
    PageId id;
    void (*on_enter)(void);
    void (*on_exit)(void);
    void (*on_key)(KeyId key, KeyEvent event);
    void (*on_render)(void);
} PageVtbl;

/* 初始化页面管理器 */
esp_err_t page_mgr_init(void);

/* 注册页面 */
esp_err_t page_mgr_register(const PageVtbl *page);

/* 切换页面（扁平） */
esp_err_t page_mgr_switch(PageId id);

/* 压栈切换 */
esp_err_t page_mgr_push(PageId id);

/* 弹栈返回 */
esp_err_t page_mgr_pop(void);

/* 获取当前页面 */
PageId page_mgr_current(void);

/* 按键分发 */
void page_mgr_handle_key(KeyId key, KeyEvent event);

#ifdef __cplusplus
}
#endif

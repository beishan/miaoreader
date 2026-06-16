/**
 * @file menu_bar.h
 * @brief 底部菜单栏（共享层）
 */
#pragma once

#include "page_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_BAR_HEIGHT 30

void menu_bar_init(void);
void menu_bar_render(void);
void menu_bar_handle_key(KeyId key, KeyEvent event);

#ifdef __cplusplus
}
#endif

/**
 * @file page_menu.h
 * @brief 阅读上下文菜单：继续阅读/跳转/添加书签/书籍信息
 */
#pragma once

#include "ui/page_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const PageVtbl page_menu_vtbl;

/**
 * @brief 设置菜单关联的书籍信息
 *
 * 需要在 push 菜单页面之前调用。
 *
 * @param book_name 书名
 * @param current_page 当前页码
 * @param total_pages 总页数
 */
void page_menu_set_book_info(const char *book_name, uint32_t current_page, uint32_t total_pages);

#ifdef __cplusplus
}
#endif

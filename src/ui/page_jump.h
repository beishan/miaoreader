/**
 * @file page_jump.h
 * @brief 跳转页码界面：数字输入
 */
#pragma once

#include "ui/page_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const PageVtbl page_jump_vtbl;

/**
 * @brief 设置跳转界面的页码信息
 *
 * @param current_page 当前页码
 * @param total_pages 总页数
 */
void page_jump_set_page_info(uint32_t current_page, uint32_t total_pages);

#ifdef __cplusplus
}
#endif

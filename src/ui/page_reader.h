#pragma once
#include "ui/page_mgr.h"

extern const PageVtbl page_reader_vtbl;

/**
 * @brief 设置当前要阅读的书籍路径
 *
 * 由书架页调用，在切换到阅读器之前设置目标书籍。
 *
 * @param file_path 书籍文件路径（如 "/sdcard/books/三体.txt"）
 */
void page_reader_set_book(const char *file_path);

/**
 * @brief 尝试从 NVS 恢复上次阅读的书籍
 *
 * 启动时调用，检查 NVS 中是否有上次阅读记录。
 *
 * @return true 有记录且文件存在，可以恢复
 * @return false 无记录或文件不存在
 */
bool page_reader_try_restore(void);

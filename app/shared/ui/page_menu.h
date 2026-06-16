#ifndef PAGE_MENU_H
#define PAGE_MENU_H

#include "page_mgr.h"
#include <stdint.h>

extern const PageVtbl page_menu_vtbl;

void page_menu_set_book_info(const char *book_name, uint32_t current_page, uint32_t total_pages);

#endif // PAGE_MENU_H

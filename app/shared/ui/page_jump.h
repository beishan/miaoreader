#ifndef PAGE_JUMP_H
#define PAGE_JUMP_H

#include "page_mgr.h"
#include <stdint.h>

extern const PageVtbl page_jump_vtbl;

void page_jump_set_page_info(uint32_t current_page, uint32_t total_pages);

#endif // PAGE_JUMP_H

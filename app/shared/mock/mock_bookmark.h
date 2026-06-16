#ifndef MOCK_BOOKMARK_H
#define MOCK_BOOKMARK_H

#include <stdint.h>
#include <stdbool.h>

int  mock_bookmark_init(void);
int  mock_bookmark_add(const char *book_name, uint32_t page);
bool mock_bookmark_exists(const char *book_name, uint32_t page);

#endif // MOCK_BOOKMARK_H

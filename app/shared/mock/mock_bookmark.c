#ifdef PC_SIMULATION

#include "mock_bookmark.h"
#include <stdio.h>
#include <string.h>

#define MAX_BOOKMARKS 100

typedef struct {
    char book_name[64];
    uint32_t page;
    bool used;
} BookmarkEntry;

static BookmarkEntry s_bookmarks[MAX_BOOKMARKS];

int mock_bookmark_init(void) {
    memset(s_bookmarks, 0, sizeof(s_bookmarks));
    return 0;
}

int mock_bookmark_add(const char *book_name, uint32_t page) {
    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (!s_bookmarks[i].used) {
            strncpy(s_bookmarks[i].book_name, book_name, 63);
            s_bookmarks[i].page = page;
            s_bookmarks[i].used = true;
            printf("[INFO][bookmark] Added: %s page %u\n", book_name, page);
            return 0;
        }
    }
    return -1;
}

bool mock_bookmark_exists(const char *book_name, uint32_t page) {
    for (int i = 0; i < MAX_BOOKMARKS; i++) {
        if (s_bookmarks[i].used &&
            strcmp(s_bookmarks[i].book_name, book_name) == 0 &&
            s_bookmarks[i].page == page) {
            return true;
        }
    }
    return false;
}

#endif // PC_SIMULATION

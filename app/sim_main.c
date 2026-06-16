/**
 * @file sim_main.c
 * @brief PC 仿真入口（使用共享页面层）
 *
 * 布局：状态栏（顶部）+ 主体区域（中间）+ 菜单栏（底部）
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include "shared/ui/page_mgr.h"
#include "shared/ui/page_boot.h"
#include "shared/ui/page_home.h"
#include "shared/ui/page_bookshelf.h"
#include "shared/ui/page_reader.h"
#include "shared/ui/page_settings.h"
#include "shared/ui/page_menu.h"
#include "shared/ui/page_jump.h"
#include "shared/ui/page_sleep.h"
#include "shared/ui/status_bar.h"
#include "shared/ui/widget.h"
#include "shared/mock/mock_rtc.h"
#include "shared/mock/mock_weather.h"
#include "shared/mock/mock_books.h"
#include "shared/mock/mock_wifi.h"
#include "shared/storage/book_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 屏幕布局 */
#define STATUS_BAR_HEIGHT   24
#define MENU_BAR_HEIGHT     30
#define CONTENT_Y           STATUS_BAR_HEIGHT
#define CONTENT_H           (RENDERER_HEIGHT - STATUS_BAR_HEIGHT - MENU_BAR_HEIGHT)

/* 菜单项 → PageId 映射 */
static const PageId menu_to_page[] = {
    PAGE_HOME,
    PAGE_BOOKSHELF,
    PAGE_SETTINGS,
};
#define MENU_COUNT (sizeof(menu_to_page) / sizeof(menu_to_page[0]))

static int s_current_menu = 0;
static bool s_in_reader = false;
static bool s_in_bookshelf = false;  /* 是否在书架选书模式 */
static int s_selected_book = 0;  /* 书架当前选中书籍索引 */
static int s_bookshelf_page = 0; /* 书架当前页码 */
static int s_reader_page = 0;   /* 阅读器当前页码 */
#define BOOKSHELF_COLS 3
#define BOOKSHELF_ROWS 2
#define BOOKSHELF_PER_PAGE (BOOKSHELF_COLS * BOOKSHELF_ROWS)  /* 6 */
#define READER_MAX_PAGES 1000  /* 最大页数 */

/* 真实书籍文本和分页数据 */
static char *s_book_text = NULL;        /* 加载的书籍文本 */
static int s_book_total_pages = 0;      /* 总页数 */
static char s_book_filename[256] = "";  /* 书籍文件名 */

/* 阅读器菜单状态 */
static bool s_menu_visible = false;
static int  s_menu_cursor  = 0;
static bool s_show_info    = false;
static const char *s_menu_items[] = {
    "\xe2\x96\xb6 \xe7\xbb\xa7\xe7\xbb\xad\xe9\x98\x85\xe8\xaf\xbb",  /* ▶ 继续阅读 */
    "\xe2\x98\x85 \xe6\xb7\xbb\xe5\x8a\xa0\xe4\xb9\xa6\xe7\xad\xbe",  /* ★ 添加书签 */
    "\xe2\x84\xb9 \xe4\xb9\xa6\xe7\xb1\x8d\xe4\xbf\xa1\xe6\x81\xaf",  /* ℹ 书籍信息 */
};
#define READER_MENU_COUNT 3

/* 分页参数 */
#define PAGE_LINES 14
#define LINE_HEIGHT 18

/* 加载书籍文本并计算分页 */
static void load_book_text(int book_index)
{
    /* 释放旧文本 */
    if (s_book_text) {
        free(s_book_text);
        s_book_text = NULL;
    }

    s_book_text = mock_books_load_text(book_index);
    s_book_total_pages = 0;

    if (!s_book_text) {
        printf("[Reader] 无法加载书籍文本\n");
        return;
    }

    /* 获取文件名用于存储 */
    const char *filename = mock_books_get_filename(book_index);
    if (filename) {
        strncpy(s_book_filename, filename, sizeof(s_book_filename) - 1);
    }

    /* 计算总页数 */
    int line_count = 0;
    const char *p = s_book_text;
    while (*p) {
        if (*p == '\n') {
            line_count++;
        }
        p++;
    }
    s_book_total_pages = (line_count + PAGE_LINES - 1) / PAGE_LINES;
    if (s_book_total_pages < 1) s_book_total_pages = 1;

    /* 恢复阅读进度 */
    int saved_page = book_storage_load_progress(s_book_filename);
    if (saved_page >= 0 && saved_page < s_book_total_pages) {
        s_reader_page = saved_page;
    } else {
        s_reader_page = 0;
    }

    printf("[Reader] 加载书籍: %s, %d 页\n", s_book_filename, s_book_total_pages);
}

/* 获取指定页的文本行 */
static void get_page_lines(int page, const char **lines, int *line_count)
{
    *line_count = 0;
    if (!s_book_text) return;

    /* 跳到目标页 */
    int current_line = 0;
    int target_start = page * PAGE_LINES;
    const char *p = s_book_text;

    while (*p && current_line < target_start) {
        if (*p == '\n') current_line++;
        p++;
    }

    /* 读取 PAGE_LINES 行 */
    int idx = 0;
    while (*p && idx < PAGE_LINES) {
        lines[idx] = p;
        while (*p && *p != '\n') p++;
        idx++;
        if (*p == '\n') p++;
    }
    *line_count = idx;
}

/* 绘制菜单栏 */
static void draw_menu_bar(void)
{
    int y = RENDERER_HEIGHT - MENU_BAR_HEIGHT;

    renderer_fill_rect(0, y, RENDERER_WIDTH, MENU_BAR_HEIGHT, RENDERER_COLOR_WHITE);
    renderer_fill_rect(0, y, RENDERER_WIDTH, 1, RENDERER_COLOR_BLACK);

    int item_width = RENDERER_WIDTH / MENU_COUNT;

    const char *labels[MENU_COUNT] = {
        " 主页 ",
        " 书架 ",
        " 设置 ",
    };

    for (int i = 0; i < (int)MENU_COUNT; i++) {
        int x = i * item_width;
        int text_x = x + (item_width - widget_text_width(labels[i])) / 2;

        bool active = (i == (int)s_current_menu && !s_in_reader);
        bool bookshelf_active = (s_in_bookshelf && i == 1);

        if (active) {
            /* 当前选中标签 */
            RendererColor bg = bookshelf_active ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK;
            renderer_fill_rect(x + 2, y + 2, item_width - 4, MENU_BAR_HEIGHT - 4, bg);
            widget_draw_text(text_x, y + 7, labels[i], RENDERER_COLOR_WHITE);
        } else {
            renderer_draw_rect(x + 2, y + 2, item_width - 4, MENU_BAR_HEIGHT - 4, RENDERER_COLOR_BLACK);
            widget_draw_text(text_x, y + 7, labels[i], RENDERER_COLOR_BLACK);
        }

        if (i < (int)MENU_COUNT - 1) {
            renderer_fill_rect(x + item_width - 1, y + 4, 1, MENU_BAR_HEIGHT - 8, RENDERER_COLOR_BLACK);
        }
    }
}

/* 自定义渲染函数 */
static void custom_render(void)
{
    char time_buf[16];
    char date_buf[32];

    renderer_clear(RENDERER_COLOR_WHITE);

    /* 状态栏（使用 mock 数据） */
    renderer_fill_rect(0, 0, RENDERER_WIDTH, STATUS_BAR_HEIGHT, RENDERER_COLOR_BLACK);

    int sb_y = (STATUS_BAR_HEIGHT - 16) / 2;  /* 16px 字体居中 */

    /* 实时时间 */
    mock_rtc_get_time_str(time_buf, sizeof(time_buf));
    widget_draw_text(8, sb_y, time_buf, RENDERER_COLOR_WHITE);

    /* 阅读模式显示书名，其他模式显示日期 */
    if (s_in_reader) {
        MockBookMeta book;
        mock_books_get_by_index(s_selected_book, &book);
        int pct = (book.totalPages > 0) ? (book.currentPage * 100) / book.totalPages : 0;
        char progress_buf[32];
        snprintf(progress_buf, sizeof(progress_buf), "%s %d%%", book.title, pct);
        int pw = widget_text_width(progress_buf);
        widget_draw_text((RENDERER_WIDTH - pw) / 2, sb_y, progress_buf, RENDERER_COLOR_WHITE);
    } else {
        mock_rtc_get_date_str(date_buf, sizeof(date_buf));
        widget_draw_text(100, sb_y, date_buf, RENDERER_COLOR_WHITE);
    }

    /* 电量 */
    widget_draw_text(RENDERER_WIDTH - 55, sb_y, "87%", RENDERER_COLOR_WHITE);

    /* WiFi 图标 */
    uint16_t wifi_icon = mock_wifi_is_connected() ? ICON_WIFI_CONNECTED : ICON_WIFI_DISCONNECTED;
    widget_draw_icon(RENDERER_WIDTH - 20, sb_y, wifi_icon, RENDERER_COLOR_WHITE);

    /* 主体内容 */
    PageId current = page_mgr_current();

    if (s_in_reader) {
        /* 阅读器：全屏显示（使用真实书籍文本） */
        renderer_fill_rect(0, CONTENT_Y, RENDERER_WIDTH, RENDERER_HEIGHT - CONTENT_Y, RENDERER_COLOR_WHITE);

        int line_h = LINE_HEIGHT;
        int text_y = CONTENT_Y + 5;

        /* 获取当前页的文本行 */
        const char *page_lines[PAGE_LINES];
        int line_count = 0;
        get_page_lines(s_reader_page, page_lines, &line_count);

        /* 渲染正文 */
        for (int i = 0; i < line_count; i++) {
            if (page_lines[i] && page_lines[i][0] != '\0') {
                /* 临时截断行尾换行符 */
                char line_buf[128];
                int li = 0;
                const char *lp = page_lines[i];
                while (*lp && *lp != '\n' && li < 126) {
                    line_buf[li++] = *lp++;
                }
                line_buf[li] = '\0';
                widget_draw_text(20, text_y + i * line_h, line_buf, RENDERER_COLOR_BLACK);
            }
        }

        /* 页码提示 */
        char page_hint[32];
        snprintf(page_hint, sizeof(page_hint), "< %d/%d >", s_reader_page + 1, s_book_total_pages);
        int hint_w = widget_text_width(page_hint);
        widget_draw_text((RENDERER_WIDTH - hint_w) / 2, RENDERER_HEIGHT - 20, page_hint, RENDERER_COLOR_BLACK);

        /* ── 菜单覆盖层 ── */
        if (s_menu_visible) {
            int mx = 80, my = 80, mw = 240, mh = 110;
            renderer_fill_rect(mx, my, mw, mh, RENDERER_COLOR_WHITE);
            renderer_draw_rect(mx, my, mw, mh, RENDERER_COLOR_BLACK);

            for (int i = 0; i < READER_MENU_COUNT; i++) {
                RendererColor c = (i == s_menu_cursor)
                                    ? RENDERER_COLOR_RED
                                    : RENDERER_COLOR_BLACK;
                widget_draw_text(mx + 20, my + 20 + i * 30,
                                 s_menu_items[i], c);
            }
        }

        /* ── 书籍信息弹窗 ── */
        if (s_show_info) {
            int mx = 50, my = 60, mw = 300, mh = 140;
            renderer_fill_rect(mx, my, mw, mh, RENDERER_COLOR_WHITE);
            renderer_draw_rect(mx, my, mw, mh, RENDERER_COLOR_BLACK);

            int ty = my + 16;
            char ibuf[64];

            /* 书名 */
            widget_draw_text(mx + 12, ty, s_book_filename, RENDERER_COLOR_BLACK);
            ty += 22;

            /* 总页数 */
            snprintf(ibuf, sizeof(ibuf), "Total pages: %d", s_book_total_pages);
            widget_draw_text(mx + 12, ty, ibuf, RENDERER_COLOR_BLACK);
            ty += 18;

            /* 当前页 / 百分比 */
            int pct = (s_book_total_pages > 0)
                        ? (s_reader_page * 100) / s_book_total_pages
                        : 0;
            snprintf(ibuf, sizeof(ibuf), "Current: %d/%d (%d%%)",
                     s_reader_page + 1, s_book_total_pages, pct);
            widget_draw_text(mx + 12, ty, ibuf, RENDERER_COLOR_BLACK);
            ty += 18;

            /* 书签数量 */
            int bm_pages[BOOKMARK_MAX_PER_BOOK];
            int bm_count = book_storage_get_bookmarks(s_book_filename,
                                                       bm_pages,
                                                       BOOKMARK_MAX_PER_BOOK);
            snprintf(ibuf, sizeof(ibuf), "Bookmarks: %d", bm_count);
            widget_draw_text(mx + 12, ty, ibuf, RENDERER_COLOR_BLACK);
        }

    } else {
        /* 非阅读器：显示内容区域 + 菜单栏 */
        renderer_fill_rect(0, CONTENT_Y, RENDERER_WIDTH, CONTENT_H, RENDERER_COLOR_WHITE);
        renderer_fill_rect(0, CONTENT_Y, 1, CONTENT_H, RENDERER_COLOR_BLACK);
        renderer_fill_rect(RENDERER_WIDTH - 1, CONTENT_Y, 1, CONTENT_H, RENDERER_COLOR_BLACK);

        switch (current) {
        case PAGE_HOME: {
            char buf[128];

            /* 天气信息 */
            mock_weather_get_summary(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 10, buf, RENDERER_COLOR_BLACK);

            mock_weather_get_temp_str(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 30, buf, RENDERER_COLOR_BLACK);

            mock_weather_get_detail_str(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 50, buf, RENDERER_COLOR_BLACK);

            /* 分割线 */
            renderer_fill_rect(20, CONTENT_Y + 75, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

            /* 阅读统计 */
            widget_draw_text(20, CONTENT_Y + 85, "阅读统计", RENDERER_COLOR_BLACK);

            mock_books_get_count_str(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 105, buf, RENDERER_COLOR_BLACK);

            mock_books_get_today_reading_str(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 125, buf, RENDERER_COLOR_BLACK);

            mock_books_get_total_reading_str(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 145, buf, RENDERER_COLOR_BLACK);

            /* 分割线 */
            renderer_fill_rect(20, CONTENT_Y + 170, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);

            /* 当前阅读 */
            mock_books_get_current_book_str(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 180, buf, RENDERER_COLOR_BLACK);

            mock_books_get_progress_str(buf, sizeof(buf));
            widget_draw_text(20, CONTENT_Y + 200, buf, RENDERER_COLOR_BLACK);

            /* 进度条 */
            int bar_x = 20, bar_y = CONTENT_Y + 225, bar_w = RENDERER_WIDTH - 40;
            renderer_draw_rect(bar_x, bar_y, bar_w, 10, RENDERER_COLOR_BLACK);
            int progress = mock_books_get_progress_percent();
            renderer_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 6, RENDERER_COLOR_BLACK);
            break;
        }

        case PAGE_BOOKSHELF: {
            /* 从 mock 数据获取书籍 */
            int book_count = mock_books_get_count();
            int cols = BOOKSHELF_COLS;
            int rows = BOOKSHELF_ROWS;
            int cell_w = 100;
            int cover_h = 65;
            int title_h = 16;
            int cell_h = cover_h + title_h;  /* 81 */
            int gap_x = 20;
            int gap_y = 12;
            int footer_h = 20;
            int avail_h = CONTENT_H - footer_h - 10;  /* 留出页码空间 */
            int start_x = (RENDERER_WIDTH - cols * cell_w - (cols - 1) * gap_x) / 2;
            int start_y = CONTENT_Y + (avail_h - rows * cell_h - (rows - 1) * gap_y) / 2;

            /* 计算分页 */
            int total_pages = (book_count + BOOKSHELF_PER_PAGE - 1) / BOOKSHELF_PER_PAGE;
            int page_start = s_bookshelf_page * BOOKSHELF_PER_PAGE;
            int page_end = page_start + BOOKSHELF_PER_PAGE;
            if (page_end > book_count) page_end = book_count;

            for (int i = page_start; i < page_end; i++) {
                int idx = i - page_start;
                int col = idx % cols;
                int row = idx / cols;
                int x = start_x + col * (cell_w + gap_x);
                int y = start_y + row * (cell_h + gap_y);
                bool selected = s_in_bookshelf && (i == s_selected_book);
                int scale = selected ? 6 : 0;  /* 选中时放大 6px */

                /* 封面（选中时红色放大） */
                RendererColor cover_color = selected ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK;
                RendererColor spine_color = selected ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK;
                renderer_fill_rect(x + 20 - scale, y + 5 - scale, 60 + scale * 2, 45 + scale * 2, cover_color);
                renderer_fill_rect(x + 22 - scale, y + 7 - scale, 56 + scale * 2, 41 + scale * 2, RENDERER_COLOR_WHITE);
                /* 书脊线条 */
                renderer_fill_rect(x + 25 - scale, y + 7 - scale, 2, 41 + scale * 2, spine_color);

                /* 书名（封面下方，限制字符数避免遮挡） */
                MockBookMeta book;
                if (mock_books_get_by_index(i, &book) == 0) {
                    char title[32] = {0};
                    int max_chars = 4;
                    int char_count = 0;
                    int byte_idx = 0;
                    const char *src = book.title;
                    while (*src && char_count < max_chars) {
                        int char_len = 1;
                        if (((unsigned char)*src & 0xE0) == 0xC0) char_len = 2;
                        else if (((unsigned char)*src & 0xF0) == 0xE0) char_len = 3;
                        else if (((unsigned char)*src & 0xF8) == 0xF0) char_len = 4;
                        if (byte_idx + char_len >= (int)sizeof(title) - 4) break;
                        for (int k = 0; k < char_len; k++) title[byte_idx++] = src[k];
                        src += char_len;
                        char_count++;
                    }
                    if (*src) { title[byte_idx++] = '.'; title[byte_idx++] = '.'; }
                    title[byte_idx] = '\0';
                    int tw = widget_text_width(title);
                    int text_x = x + (cell_w - tw) / 2;
                    widget_draw_text(text_x, y + cover_h + 2, title, selected ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK);
                }
            }

            /* 页码 + 提示（内容区底部） */
            char page_info[64];
            if (s_in_bookshelf) {
                snprintf(page_info, sizeof(page_info), "第 %d/%d 页  共 %d 本  [ESC退出]",
                         s_bookshelf_page + 1, total_pages, book_count);
            } else {
                snprintf(page_info, sizeof(page_info), "第 %d/%d 页  共 %d 本  [确认选书]",
                         s_bookshelf_page + 1, total_pages, book_count);
            }
            int info_w = widget_text_width(page_info);
            widget_draw_text((RENDERER_WIDTH - info_w) / 2, CONTENT_Y + CONTENT_H - 30, page_info, RENDERER_COLOR_BLACK);
            break;
        }

        case PAGE_SETTINGS:
            widget_draw_text(20, CONTENT_Y + 10, "设置", RENDERER_COLOR_BLACK);
            renderer_fill_rect(20, CONTENT_Y + 30, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 40, "阅读排版", RENDERER_COLOR_RED);
            widget_draw_text(20, CONTENT_Y + 60, "  字体：思源宋体", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 80, "  字号：20px", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 100, "  行距：1.5", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 120, "  字距：1px", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 140, "  页边距：8px", RENDERER_COLOR_BLACK);
            renderer_fill_rect(20, CONTENT_Y + 160, RENDERER_WIDTH - 40, 1, RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 170, "系统设置", RENDERER_COLOR_RED);
            widget_draw_text(20, CONTENT_Y + 190, "  休眠：时钟天气", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 210, "  超时：5 分钟", RENDERER_COLOR_BLACK);
            widget_draw_text(20, CONTENT_Y + 230, "  版本：v0.5.0", RENDERER_COLOR_BLACK);
            break;

        default:
            break;
        }

        /* 菜单栏 */
        draw_menu_bar();
    }

    renderer_display();
}

/* 按键回调 */
static void on_key(RendererKeyId key, RendererKeyEvent event)
{
    /* 映射 RendererKeyId → KeyId（预留，供 page_mgr_handle_key 使用） */
    KeyId page_key;
    switch (key) {
    case RENDERER_KEY_PWR:  page_key = KEY_PWR;  break;
    case RENDERER_KEY_PREV: page_key = KEY_PREV; break;
    case RENDERER_KEY_NEXT: page_key = KEY_NEXT; break;
    case RENDERER_KEY_HOME: page_key = KEY_HOME; break;
    default: return;
    }
    (void)page_key;  /* 暂时保留映射，后续迁移到 page_mgr_handle_key 时使用 */

    /* 映射 RendererKeyEvent → KeyEvent */
    KeyEvent page_event;
    switch (event) {
    case RENDERER_KEY_EVT_SHORT:      page_event = KEY_EVT_SHORT;      break;
    case RENDERER_KEY_EVT_LONG:       page_event = KEY_EVT_LONG;       break;
    case RENDERER_KEY_EVT_SUPER_LONG: page_event = KEY_EVT_SUPER_LONG; break;
    case RENDERER_KEY_EVT_COMBO:      page_event = KEY_EVT_COMBO;      break;
    default: return;
    }

    /* 阅读器模式 */
    if (s_in_reader) {
        /* 信息弹窗：任意键关闭 */
        if (s_show_info) {
            if (key == RENDERER_KEY_HOME || key == RENDERER_KEY_PWR) {
                s_show_info = false;
                custom_render();
            }
            return;
        }
        /* 菜单模式：←/→ 移动光标，HOME 确认，PWR 关闭 */
        if (s_menu_visible) {
            if (page_event != KEY_EVT_SHORT) return;
            switch (key) {
            case RENDERER_KEY_PREV:
                if (s_menu_cursor > 0) s_menu_cursor--;
                else                   s_menu_cursor = READER_MENU_COUNT - 1;
                custom_render();
                break;
            case RENDERER_KEY_NEXT:
                if (s_menu_cursor < READER_MENU_COUNT - 1) s_menu_cursor++;
                else                                        s_menu_cursor = 0;
                custom_render();
                break;
            case RENDERER_KEY_HOME:
                switch (s_menu_cursor) {
                case 0: /* 继续阅读 */
                    s_menu_visible = false;
                    custom_render();
                    break;
                case 1: /* 添加书签 */
                    if (s_book_filename[0]) {
                        book_storage_add_bookmark(s_book_filename, s_reader_page);
                        book_storage_save();
                    }
                    s_menu_visible = false;
                    custom_render();
                    break;
                case 2: /* 书籍信息 */
                    s_menu_visible = false;
                    s_show_info    = true;
                    custom_render();
                    break;
                }
                break;
            case RENDERER_KEY_PWR:
                s_menu_visible = false;
                custom_render();
                break;
            default:
                break;
            }
            return;
        }
        /* 正常阅读模式 */
        if (page_event == KEY_EVT_SHORT) {
            /* 短按：PWR 退出，HOME 打开菜单，←/→ 翻页 */
            switch (key) {
            case RENDERER_KEY_PWR:
                /* 保存阅读进度 */
                if (s_book_filename[0]) {
                    book_storage_save_progress(s_book_filename, s_reader_page);
                    book_storage_save();
                }
                s_in_reader = false;
                s_in_bookshelf = true;
                s_current_menu = 1; /* 回到书架菜单 */
                break;
            case RENDERER_KEY_HOME:
                s_menu_visible = true;
                s_menu_cursor  = 0;
                break;
            case RENDERER_KEY_PREV:
                if (s_reader_page > 0) s_reader_page--;
                break;
            case RENDERER_KEY_NEXT:
                if (s_reader_page < s_book_total_pages - 1) s_reader_page++;
                break;
            default:
                break;
            }
        } else if (page_event == KEY_EVT_LONG) {
            /* 长按 ←：跳到第一页；长按 →：跳到最后一页 */
            switch (key) {
            case RENDERER_KEY_PREV:
                s_reader_page = 0;
                break;
            case RENDERER_KEY_NEXT:
                s_reader_page = s_book_total_pages - 1;
                break;
            default:
                break;
            }
        }
        custom_render();
        return;
    }

    /* 书架选书模式 */
    if (s_in_bookshelf) {
        if (page_event == KEY_EVT_SHORT) {
            int book_count = mock_books_get_count();
            int total_pages = (book_count + BOOKSHELF_PER_PAGE - 1) / BOOKSHELF_PER_PAGE;
            switch (key) {
            case RENDERER_KEY_PREV:
                s_selected_book--;
                if (s_selected_book < 0) {
                    s_selected_book = book_count - 1;
                    s_bookshelf_page = total_pages - 1;
                } else if (s_selected_book < s_bookshelf_page * BOOKSHELF_PER_PAGE) {
                    s_bookshelf_page--;
                }
                break;
            case RENDERER_KEY_NEXT:
                s_selected_book++;
                if (s_selected_book >= book_count) {
                    s_selected_book = 0;
                    s_bookshelf_page = 0;
                } else if (s_selected_book >= (s_bookshelf_page + 1) * BOOKSHELF_PER_PAGE) {
                    s_bookshelf_page++;
                }
                break;
            case RENDERER_KEY_PWR:
                /* ESC：退出书架选书模式 */
                s_in_bookshelf = false;
                break;
            case RENDERER_KEY_HOME:
                /* HOME：打开选中的书 */
                load_book_text(s_selected_book);
                s_in_reader = true;
                s_menu_visible = false;
                s_show_info = false;
                break;
            default:
                break;
            }
        }
        custom_render();
        return;
    }

    /* 标签模式 */
    if (page_event == KEY_EVT_SHORT) {
        switch (key) {
        case RENDERER_KEY_PWR:
            break;

        case RENDERER_KEY_PREV:
            s_current_menu = (s_current_menu > 0) ? s_current_menu - 1 : (int)MENU_COUNT - 1;
            page_mgr_switch(menu_to_page[s_current_menu]);
            break;

        case RENDERER_KEY_NEXT:
            s_current_menu = (s_current_menu < (int)MENU_COUNT - 1) ? s_current_menu + 1 : 0;
            page_mgr_switch(menu_to_page[s_current_menu]);
            break;

        case RENDERER_KEY_HOME:
            if (menu_to_page[s_current_menu] == PAGE_BOOKSHELF) {
                /* 进入书架选书模式 */
                s_in_bookshelf = true;
            } else if (menu_to_page[s_current_menu] == PAGE_HOME) {
                /* 主页按 HOME 进入设置 */
                s_current_menu = 2;
                page_mgr_switch(PAGE_SETTINGS);
            }
            break;

        default:
            break;
        }
    }

    custom_render();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== 妙阅 E-Reader PC 仿真 ===\n\n");
    printf("按键:\n");
    printf("  ESC   - 退出选书模式 / 退出阅读 / 退出仿真\n");
    printf("  LEFT  - 上一个标签 / 上一本书 / 菜单上移\n");
    printf("  RIGHT - 下一个标签 / 下一本书 / 菜单下移\n");
    printf("  ENTER - 确认进入书架 / 打开书籍 / 打开菜单\n\n");

    /* 初始化渲染器 */
    if (renderer_init("妙阅 E-Reader 仿真") != 0) {
        fprintf(stderr, "渲染器初始化失败\n");
        return 1;
    }

    /* 初始化 Mock 数据层 */
    mock_rtc_init();
    mock_weather_init();
    book_storage_init();
    mock_books_init();
    mock_wifi_init();

    /* 初始化页面管理器 */
    page_mgr_init();
    widget_init();
    status_bar_init();

    /* 注册页面 */
    page_mgr_register(&page_boot_vtbl);
    page_mgr_register(&page_home_vtbl);
    page_mgr_register(&page_bookshelf_vtbl);
    page_mgr_register(&page_reader_vtbl);
    page_mgr_register(&page_settings_vtbl);
    page_mgr_register(&page_menu_vtbl);
    page_mgr_register(&page_jump_vtbl);
    page_mgr_register(&page_sleep_vtbl);

    /* 设置按键回调 */
    renderer_set_key_callback(on_key);

    /* 进入主页 */
    page_mgr_switch(PAGE_HOME);

    /* 初始渲染 */
    custom_render();

    /* 事件循环 */
    printf("开始事件循环...\n");
    while (1) {
        if (renderer_poll_events() != 0) {
            break;
        }
        renderer_delay(16);
    }

    printf("退出仿真\n");
    return 0;
}

#endif /* PC_SIMULATION */

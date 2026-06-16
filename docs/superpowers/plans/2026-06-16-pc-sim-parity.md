# PC 仿真与 ESP32 一致性改进实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 让 PC 仿真（app/）与 ESP32 真实代码（src/）达到 95% 以上的一致性，实现排版引擎、缺失页面、GBK 映射、架构统一、长按支持。

**架构：** 将 ESP32 的 typesetter 引擎移植到 PC 仿真层（使用系统 FreeType 替代嵌入式 FreeType），补充 4 个缺失页面（boot/menu/jump/sleep），统一 book_parser 的 GBK 映射表，重构 sim_main.c 移除双轨渲染，增加长按事件支持。

**技术栈：** SDL2、FreeType、C99、PC_SIMULATION 条件编译

---

## 文件结构

### 新建文件

| 文件 | 职责 |
|------|------|
| `app/shared/engine/typesetter.h` | 排版引擎接口（PageIndex、TypesetterConfig、API） |
| `app/shared/engine/typesetter.c` | 排版引擎实现（分页算法、FreeType 渲染） |
| `app/shared/engine/gbk_table.h` | GBK→Unicode 精确映射表（23940 项） |
| `app/shared/ui/page_boot.h` | 首次配网引导页接口 |
| `app/shared/ui/page_boot.c` | 配网引导页实现 |
| `app/shared/ui/page_menu.h` | 阅读上下文菜单接口 |
| `app/shared/ui/page_menu.c` | 阅读上下文菜单实现 |
| `app/shared/ui/page_jump.h` | 跳转页码界面接口 |
| `app/shared/ui/page_jump.c` | 跳转页码界面实现 |
| `app/shared/ui/page_sleep.h` | 休眠界面接口 |
| `app/shared/ui/page_sleep.c` | 休眠界面实现 |
| `app/shared/mock/mock_bookmark.h` | 书签 Mock 接口 |
| `app/shared/mock/mock_bookmark.c` | 书签 Mock 实现 |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `app/shared/engine/book_parser.c` | 替换简化 GBK 映射为精确映射表 |
| `app/shared/ui/page_reader.c` | 集成 typesetter 排版引擎 |
| `app/shared/ui/page_settings.c` | 添加系统设置子菜单 |
| `app/sim_main.c` | 移除双轨渲染，增加长按支持，注册新页面 |
| `app/Makefile` | 添加新文件到编译列表 |
| `app/renderer_if.h` | 添加长按时间常量定义 |
| `app/renderer_sdl.c` | 实现长按事件检测 |

---

## 任务 1：移植 typesetter 排版引擎

**文件：**
- 创建：`app/shared/engine/typesetter.h`
- 创建：`app/shared/engine/typesetter.c`
- 修改：`app/Makefile`

- [ ] **步骤 1：创建 typesetter.h 接口定义**

```c
// app/shared/engine/typesetter.h
#ifndef TYPESETTER_H
#define TYPESETTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 预分页索引条目
typedef struct {
    uint32_t startByte;   // 页起始字节偏移
    uint16_t startLine;   // 页起始行号
    uint16_t pageNum;     // 页码（1-based）
} PageIndex;

// 排版配置
typedef struct {
    uint8_t  fontId;       // 字体索引（0=思源宋体 1=思源黑体 2=文楷）
    uint8_t  fontSize;     // 字号 px
    uint8_t  lineSpacing;  // 行距倍数 x10（15=1.5倍）
    uint8_t  charSpacing;  // 字间距 px
    uint8_t  margin;       // 页边距 px
    int      pageWidth;    // 可用宽度 px
    int      pageHeight;   // 可用高度 px
} TypesetterConfig;

// 默认配置
#define TYPESETTER_DEFAULT_FONT_SIZE     20
#define TYPESETTER_DEFAULT_LINE_SPACING  15  // 1.5倍
#define TYPESETTER_DEFAULT_CHAR_SPACING  1
#define TYPESETTER_DEFAULT_MARGIN        12

// API
int  typesetter_register_font(int fontId, const char *ttfPath);
int  typesetter_init(const TypesetterConfig *cfg);
int  typesetter_paginate(const char *text, uint32_t textLen,
                         PageIndex **pagesOut, uint32_t *pageCountOut);
void typesetter_render_page(const char *text, const PageIndex *page,
                            uint8_t *epdBuf, int bufW, int bufH, int yOffset);
void typesetter_free_pages(PageIndex *pages);
void typesetter_reload_config(const TypesetterConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif // TYPESETTER_H
```

- [ ] **步骤 2：创建 typesetter.c 核心实现**

```c
// app/shared/engine/typesetter.c
#ifdef PC_SIMULATION

#include "typesetter.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FONTS      4
#define MAX_FONT_PATH  256
#define INITIAL_PAGES  64

// 全局状态
static char s_font_paths[MAX_FONTS][MAX_FONT_PATH];
static FT_Library s_ft = NULL;
static FT_Face s_face = NULL;
static int s_current_font_id = -1;
static TypesetterConfig s_cfg = {
    .fontSize = TYPESETTER_DEFAULT_FONT_SIZE,
    .lineSpacing = TYPESETTER_DEFAULT_LINE_SPACING,
    .charSpacing = TYPESETTER_DEFAULT_CHAR_SPACING,
    .margin = TYPESETTER_DEFAULT_MARGIN,
    .pageWidth = 376,
    .pageHeight = 262,
};

// 获取 UTF-8 字符的字节数
static int utf8_char_bytes(unsigned char c) {
    if (c < 0x80) return 1;
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

// 从 UTF-8 解码 Unicode 码点
static uint32_t utf8_decode(const char *s, int *bytes) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *bytes = 1; return c; }
    if (c < 0xE0) { *bytes = 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if (c < 0xF0) { *bytes = 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    *bytes = 4;
    return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

// 加载字体
static int load_font(int fontId) {
    if (fontId == s_current_font_id && s_face) return 0;
    if (fontId < 0 || fontId >= MAX_FONTS) return -1;
    if (s_font_paths[fontId][0] == '\0') return -1;

    if (s_face) {
        FT_Done_Face(s_face);
        s_face = NULL;
    }

    FT_Error err = FT_New_Face(s_ft, s_font_paths[fontId], 0, &s_face);
    if (err) {
        printf("[ERROR][typesetter] Failed to load font %d: %d\n", fontId, err);
        return -1;
    }

    FT_Set_Pixel_Sizes(s_face, 0, s_cfg.fontSize);
    s_current_font_id = fontId;
    return 0;
}

// 获取字符宽度
static int char_width(uint32_t codepoint) {
    if (!s_face) return s_cfg.fontSize / 2;
    if (FT_Load_Char(s_face, codepoint, FT_LOAD_DEFAULT) == 0) {
        return s_face->glyph->advance.x >> 6;
    }
    return s_cfg.fontSize / 2;
}

int typesetter_register_font(int fontId, const char *ttfPath) {
    if (fontId < 0 || fontId >= MAX_FONTS || !ttfPath) return -1;
    strncpy(s_font_paths[fontId], ttfPath, MAX_FONT_PATH - 1);
    s_font_paths[fontId][MAX_FONT_PATH - 1] = '\0';
    return 0;
}

int typesetter_init(const TypesetterConfig *cfg) {
    if (cfg) {
        s_cfg = *cfg;
    }

    if (!s_ft) {
        FT_Error err = FT_Init_FreeType(&s_ft);
        if (err) {
            printf("[ERROR][typesetter] FT_Init_FreeType failed: %d\n", err);
            return -1;
        }
    }

    // 加载默认字体
    if (s_cfg.fontId < MAX_FONTS && s_font_paths[s_cfg.fontId][0] != '\0') {
        load_font(s_cfg.fontId);
    }

    return 0;
}

int typesetter_paginate(const char *text, uint32_t textLen,
                        PageIndex **pagesOut, uint32_t *pageCountOut) {
    if (!text || !pagesOut || !pageCountOut) return -1;

    load_font(s_cfg.fontId);

    int avail_w = s_cfg.pageWidth;
    int line_h = s_cfg.fontSize * s_cfg.lineSpacing / 10;
    int lines_per_page = s_cfg.pageHeight / line_h;
    if (lines_per_page < 1) lines_per_page = 1;

    uint32_t capacity = INITIAL_PAGES;
    PageIndex *pages = (PageIndex *)malloc(capacity * sizeof(PageIndex));
    if (!pages) return -1;

    uint32_t page_count = 0;
    uint32_t line_in_page = 0;
    uint32_t line_num = 0;
    uint32_t i = 0;

    // 第一页起始
    pages[page_count].startByte = 0;
    pages[page_count].startLine = 0;
    pages[page_count].pageNum = page_count + 1;
    page_count++;

    while (i < textLen) {
        if (text[i] == '\n') {
            i++;
            line_in_page++;
            line_num++;
        } else {
            // 累积一行
            int w = 0;
            uint32_t line_start = i;
            while (i < textLen && text[i] != '\n') {
                int bytes = 1;
                uint32_t cp = utf8_decode(text + i, &bytes);
                int cw = char_width(cp) + s_cfg.charSpacing;
                if (w + cw > avail_w && i > line_start) break;
                w += cw;
                i += bytes;
            }
            line_in_page++;
            line_num++;
        }

        // 分页判定
        if (line_in_page >= lines_per_page && i < textLen) {
            if (page_count >= capacity) {
                capacity *= 2;
                PageIndex *new_pages = (PageIndex *)realloc(pages, capacity * sizeof(PageIndex));
                if (!new_pages) { free(pages); return -1; }
                pages = new_pages;
            }
            pages[page_count].startByte = i;
            pages[page_count].startLine = line_num;
            pages[page_count].pageNum = page_count + 1;
            page_count++;
            line_in_page = 0;
        }
    }

    *pagesOut = pages;
    *pageCountOut = page_count;
    return 0;
}

void typesetter_render_page(const char *text, const PageIndex *page,
                            uint8_t *epdBuf, int bufW, int bufH, int yOffset) {
    if (!text || !page) return;

    load_font(s_cfg.fontId);

    int x = s_cfg.margin;
    int y = yOffset + s_cfg.margin;
    int line_h = s_cfg.fontSize * s_cfg.lineSpacing / 10;
    uint32_t i = page->startByte;

    // 使用 renderer 接口绘制
    extern void renderer_set_pixel(int x, int y, int color);
    extern void renderer_clear(int color);

    while (text[i] != '\0') {
        if (text[i] == '\n') {
            x = s_cfg.margin;
            y += line_h;
            i++;
            if (y >= yOffset + s_cfg.pageHeight) break;
            continue;
        }

        int bytes = 1;
        uint32_t cp = utf8_decode(text + i, &bytes);

        if (s_face && FT_Load_Char(s_face, cp, FT_LOAD_RENDER) == 0) {
            FT_GlyphSlot glyph = s_face->glyph;
            int gx = x + glyph->bitmap_left;
            int gy = y + s_cfg.fontSize - glyph->bitmap_top;

            for (int row = 0; row < glyph->bitmap.rows; row++) {
                for (int col = 0; col < glyph->bitmap.width; col++) {
                    uint8_t val = glyph->bitmap.buffer[row * glyph->bitmap.pitch + col];
                    if (val > 128) {
                        int px = gx + col;
                        int py = gy + row;
                        if (px >= 0 && px < bufW && py >= 0 && py < bufH) {
                            renderer_set_pixel(px, py, 0); // 黑色
                        }
                    }
                }
            }
            x += glyph->advance.x >> 6;
        } else {
            // 回退：简单矩形
            renderer_fill_rect(x, y, s_cfg.fontSize / 2, s_cfg.fontSize, 0);
            x += s_cfg.fontSize / 2;
        }

        i += bytes;
        if (x >= bufW - s_cfg.margin) {
            x = s_cfg.margin;
            y += line_h;
            if (y >= yOffset + s_cfg.pageHeight) break;
        }
    }
}

void typesetter_free_pages(PageIndex *pages) {
    if (pages) free(pages);
}

void typesetter_reload_config(const TypesetterConfig *cfg) {
    if (cfg) {
        s_cfg = *cfg;
        s_current_font_id = -1; // 强制重新加载字体
    }
}

#endif // PC_SIMULATION
```

- [ ] **步骤 3：更新 Makefile 添加 typesetter 编译**

在 `app/Makefile` 的 SRCS 列表中添加：
```
shared/engine/typesetter.c
```

- [ ] **步骤 4：编译验证**

```bash
cd app && make clean && make
```

预期：编译通过，无错误

- [ ] **步骤 5：Commit**

```bash
git add app/shared/engine/typesetter.h app/shared/engine/typesetter.c app/Makefile
git commit -m "feat(pc-sim): add typesetter engine for real pagination"
```

---

## 任务 2：统一 book_parser GBK 映射

**文件：**
- 创建：`app/shared/engine/gbk_table.h`
- 修改：`app/shared/engine/book_parser.c`

- [ ] **步骤 1：复制 gbk_table.h 到 PC 仿真目录**

从 `src/engine/gbk_table.h` 复制到 `app/shared/engine/gbk_table.h`

```bash
cp src/engine/gbk_table.h app/shared/engine/gbk_table.h
```

- [ ] **步骤 2：修改 book_parser.c 使用精确映射表**

在 `app/shared/engine/book_parser.c` 顶部添加：
```c
#include "gbk_table.h"
```

找到 GBK 转换函数（约 line 963），替换简化映射为：
```c
// 旧代码：简化映射
// uint32_t unicode = (c1 - 0x81) * 191 + (c2 - 0x40) + 0x4E00;

// 新代码：精确映射
int row = c1 - 0x81;
int col = c2 - 0x40;
if (c2 > 0x7F) col--;
int idx = row * 190 + col;
uint32_t unicode = 0;
if (idx >= 0 && idx < 23940) {
    unicode = gbk_to_unicode[idx];
}
if (unicode == 0) {
    unicode = 0xFFFD; // 替换字符
}
```

- [ ] **步骤 3：编译验证**

```bash
cd app && make clean && make
```

预期：编译通过，GBK 映射精度与 ESP32 一致

- [ ] **步骤 4：Commit**

```bash
git add app/shared/engine/gbk_table.h app/shared/engine/book_parser.c
git commit -m "fix(pc-sim): use precise GBK mapping table from ESP32"
```

---

## 任务 3：补充缺失页面 — page_boot

**文件：**
- 创建：`app/shared/ui/page_boot.h`
- 创建：`app/shared/ui/page_boot.c`
- 修改：`app/sim_main.c`

- [ ] **步骤 1：创建 page_boot.h**

```c
// app/shared/ui/page_boot.h
#ifndef PAGE_BOOT_H
#define PAGE_BOOT_H

#include "page_mgr.h"

extern const PageVtbl page_boot_vtbl;

#endif // PAGE_BOOT_H
```

- [ ] **步骤 2：创建 page_boot.c**

```c
// app/shared/ui/page_boot.c
#ifdef PC_SIMULATION

#include "page_boot.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "status_bar.h"
#include <stdio.h>
#include <string.h>

static void on_enter(void) {
    // 无操作
}

static void on_key(KeyId key, KeyEvent event) {
    // 配网页面不响应按键
}

static void on_render(void) {
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int cx = RENDERER_WIDTH / 2;

    // 标题
    const char *title = "欢迎使用 妙阅";
    int tw = renderer_text_width(title);
    renderer_draw_text(cx - tw / 2, 40, title, RENDERER_COLOR_BLACK);

    // 分割线
    renderer_draw_rect(40, 60, RENDERER_WIDTH - 80, 1, RENDERER_COLOR_BLACK);

    // 配网提示
    const char *lines[] = {
        "请连接以下 WiFi 进行配网:",
        "SSID: EReader-SIM",
        "密码: ereader123",
        "然后访问 http://192.168.4.1",
        "配置 WiFi 和天气信息",
    };
    int y = 80;
    for (int i = 0; i < 5; i++) {
        int lw = renderer_text_width(lines[i]);
        renderer_draw_text(cx - lw / 2, y, lines[i], RENDERER_COLOR_BLACK);
        y += 25;
    }

    // 底部提示
    const char *hint = "按任意键跳过配网";
    int hw = renderer_text_width(hint);
    renderer_draw_text(cx - hw / 2, RENDERER_HEIGHT - 30, hint, RENDERER_COLOR_BLACK);

    renderer_display();
}

const PageVtbl page_boot_vtbl = {
    .id = PAGE_BOOT,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#endif // PC_SIMULATION
```

- [ ] **步骤 3：在 sim_main.c 中注册 page_boot**

在 `main()` 函数中，`page_mgr_register(page_home_vtbl)` 之前添加：
```c
#include "page_boot.h"
// ...
page_mgr_register(page_boot_vtbl);
```

- [ ] **步骤 4：编译验证**

```bash
cd app && make clean && make
```

- [ ] **步骤 5：Commit**

```bash
git add app/shared/ui/page_boot.h app/shared/ui/page_boot.c app/sim_main.c
git commit -m "feat(pc-sim): add page_boot for first-time setup screen"
```

---

## 任务 4：补充缺失页面 — page_menu

**文件：**
- 创建：`app/shared/ui/page_menu.h`
- 创建：`app/shared/ui/page_menu.c`
- 创建：`app/shared/mock/mock_bookmark.h`
- 创建：`app/shared/mock/mock_bookmark.c`
- 修改：`app/sim_main.c`
- 修改：`app/Makefile`

- [ ] **步骤 1：创建 mock_bookmark.h**

```c
// app/shared/mock/mock_bookmark.h
#ifndef MOCK_BOOKMARK_H
#define MOCK_BOOKMARK_H

#include <stdint.h>
#include <stdbool.h>

int  mock_bookmark_init(void);
int  mock_bookmark_add(const char *book_name, uint32_t page);
bool mock_bookmark_exists(const char *book_name, uint32_t page);

#endif // MOCK_BOOKMARK_H
```

- [ ] **步骤 2：创建 mock_bookmark.c**

```c
// app/shared/mock/mock_bookmark.c
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
```

- [ ] **步骤 3：创建 page_menu.h**

```c
// app/shared/ui/page_menu.h
#ifndef PAGE_MENU_H
#define PAGE_MENU_H

#include "page_mgr.h"
#include <stdint.h>

extern const PageVtbl page_menu_vtbl;

void page_menu_set_book_info(const char *book_name, uint32_t current_page, uint32_t total_pages);

#endif // PAGE_MENU_H
```

- [ ] **步骤 4：创建 page_menu.c**

```c
// app/shared/ui/page_menu.c
#ifdef PC_SIMULATION

#include "page_menu.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "status_bar.h"
#include "mock_bookmark.h"
#include <stdio.h>
#include <string.h>

static char s_book_name[64] = {0};
static uint32_t s_current_page = 0;
static uint32_t s_total_pages = 0;
static int s_selected = 0;

static const char *menu_items[] = {
    "继续阅读",
    "跳转到...",
    "添加书签",
    "书籍信息",
};
#define MENU_ITEMS_COUNT 4

void page_menu_set_book_info(const char *book_name, uint32_t current_page, uint32_t total_pages) {
    if (book_name) strncpy(s_book_name, book_name, 63);
    s_current_page = current_page;
    s_total_pages = total_pages;
    s_selected = 0;
}

static void on_enter(void) {
    s_selected = 0;
}

static void on_key(KeyId key, KeyEvent event) {
    if (event != KEY_EVT_SHORT) return;

    switch (key) {
        case KEY_PREV:
            if (s_selected > 0) {
                s_selected--;
                page_mgr_current(); // 触发重绘
            }
            break;
        case KEY_NEXT:
            if (s_selected < MENU_ITEMS_COUNT - 1) {
                s_selected++;
                page_mgr_current(); // 触发重绘
            }
            break;
        case KEY_HOME:
            switch (s_selected) {
                case 0: // 继续阅读
                    page_mgr_pop();
                    break;
                case 1: // 跳转到
                    // TODO: 推入 page_jump
                    page_mgr_pop();
                    break;
                case 2: // 添加书签
                    mock_bookmark_add(s_book_name, s_current_page);
                    page_mgr_pop();
                    break;
                case 3: // 书籍信息
                    page_mgr_pop();
                    break;
            }
            break;
        case KEY_PWR:
            page_mgr_pop();
            break;
        default:
            break;
    }
}

static void on_render(void) {
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int cx = RENDERER_WIDTH / 2;

    // 标题
    const char *title = "阅读菜单";
    int tw = renderer_text_width(title);
    renderer_draw_text(cx - tw / 2, 40, title, RENDERER_COLOR_BLACK);

    // 分割线
    renderer_draw_rect(40, 60, RENDERER_WIDTH - 80, 1, RENDERER_COLOR_BLACK);

    // 菜单项
    int y = 80;
    for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
        char buf[80];
        if (i == s_selected) {
            snprintf(buf, sizeof(buf), "> %s", menu_items[i]);
            renderer_draw_text(60, y, buf, RENDERER_COLOR_RED);
        } else {
            snprintf(buf, sizeof(buf), "  %s", menu_items[i]);
            renderer_draw_text(60, y, buf, RENDERER_COLOR_BLACK);
        }
        y += 30;
    }

    // 底部页码
    char page_info[32];
    snprintf(page_info, sizeof(page_info), "第 %u/%u 页", s_current_page + 1, s_total_pages);
    int pw = renderer_text_width(page_info);
    renderer_draw_text(cx - pw / 2, RENDERER_HEIGHT - 30, page_info, RENDERER_COLOR_BLACK);

    renderer_display();
}

const PageVtbl page_menu_vtbl = {
    .id = PAGE_MENU,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#endif // PC_SIMULATION
```

- [ ] **步骤 5：更新 Makefile 并注册页面**

在 `app/Makefile` 的 SRCS 列表中添加：
```
shared/mock/mock_bookmark.c
shared/ui/page_menu.c
```

在 `app/sim_main.c` 中添加：
```c
#include "page_menu.h"
// ...
page_mgr_register(page_menu_vtbl);
```

- [ ] **步骤 6：编译验证并 Commit**

```bash
cd app && make clean && make
git add -A
git commit -m "feat(pc-sim): add page_menu with bookmark support"
```

---

## 任务 5：补充缺失页面 — page_jump

**文件：**
- 创建：`app/shared/ui/page_jump.h`
- 创建：`app/shared/ui/page_jump.c`
- 修改：`app/sim_main.c`
- 修改：`app/Makefile`

- [ ] **步骤 1：创建 page_jump.h**

```c
// app/shared/ui/page_jump.h
#ifndef PAGE_JUMP_H
#define PAGE_JUMP_H

#include "page_mgr.h"
#include <stdint.h>

extern const PageVtbl page_jump_vtbl;

void page_jump_set_page_info(uint32_t current_page, uint32_t total_pages);

#endif // PAGE_JUMP_H
```

- [ ] **步骤 2：创建 page_jump.c**

```c
// app/shared/ui/page_jump.c
#ifdef PC_SIMULATION

#include "page_jump.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "status_bar.h"
#include <stdio.h>
#include <string.h>

static uint32_t s_current_page = 0;
static uint32_t s_total_pages = 0;
static int s_digits[4] = {0};
static int s_digit_count = 0;

void page_jump_set_page_info(uint32_t current_page, uint32_t total_pages) {
    s_current_page = current_page;
    s_total_pages = total_pages;
    memset(s_digits, 0, sizeof(s_digits));
    s_digit_count = 0;
}

static void on_enter(void) {
    memset(s_digits, 0, sizeof(s_digits));
    s_digit_count = 0;
}

static uint32_t get_target_page(void) {
    uint32_t target = 0;
    for (int i = 0; i < s_digit_count; i++) {
        target = target * 10 + s_digits[i];
    }
    return target;
}

static void on_key(KeyId key, KeyEvent event) {
    if (event != KEY_EVT_SHORT) return;

    switch (key) {
        case KEY_PREV:
            if (s_digit_count > 0) {
                s_digits[s_digit_count - 1]--;
                if (s_digits[s_digit_count - 1] < 0) {
                    s_digits[s_digit_count - 1] = 9;
                }
            }
            break;
        case KEY_NEXT:
            if (s_digit_count == 0) {
                s_digits[0] = 1;
                s_digit_count = 1;
            } else {
                s_digits[s_digit_count - 1]++;
                if (s_digits[s_digit_count - 1] > 9) {
                    s_digits[s_digit_count - 1] = 0;
                    if (s_digit_count < 4) {
                        s_digit_count++;
                        s_digits[s_digit_count - 1] = 1;
                    }
                }
            }
            break;
        case KEY_HOME: {
            uint32_t target = get_target_page();
            if (target < 1) target = 1;
            if (target > s_total_pages) target = s_total_pages;
            // 返回阅读器（pop 两次：jump -> menu -> reader）
            page_mgr_pop();
            page_mgr_pop();
            break;
        }
        case KEY_PWR:
            page_mgr_pop();
            break;
        default:
            break;
    }

    // 触发重绘
    on_render();
}

static void on_render(void) {
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int cx = RENDERER_WIDTH / 2;

    // 标题
    const char *title = "跳转到页码";
    int tw = renderer_text_width(title);
    renderer_draw_text(cx - tw / 2, 40, title, RENDERER_COLOR_BLACK);

    // 分割线
    renderer_draw_rect(40, 60, RENDERER_WIDTH - 80, 1, RENDERER_COLOR_BLACK);

    // 输入显示
    char input_buf[16] = {0};
    for (int i = 0; i < s_digit_count; i++) {
        input_buf[i] = '0' + s_digits[i];
    }
    input_buf[s_digit_count] = '_';
    input_buf[s_digit_count + 1] = '\0';
    int iw = renderer_text_width(input_buf);
    renderer_draw_text(cx - iw / 2, 100, input_buf, RENDERER_COLOR_BLACK);

    // 范围提示
    char range_buf[32];
    snprintf(range_buf, sizeof(range_buf), "范围: 1 - %u", s_total_pages);
    int rw = renderer_text_width(range_buf);
    renderer_draw_text(cx - rw / 2, 130, range_buf, RENDERER_COLOR_BLACK);

    // 当前页码
    char curr_buf[32];
    snprintf(curr_buf, sizeof(curr_buf), "当前: 第 %u 页", s_current_page + 1);
    int cw = renderer_text_width(curr_buf);
    renderer_draw_text(cx - cw / 2, 160, curr_buf, RENDERER_COLOR_BLACK);

    // 操作提示
    const char *hints[] = {
        "PREV/NEXT: 调整数字",
        "HOME: 确认跳转",
        "PWR: 取消返回",
    };
    int y = 200;
    for (int i = 0; i < 3; i++) {
        int hw = renderer_text_width(hints[i]);
        renderer_draw_text(cx - hw / 2, y, hints[i], RENDERER_COLOR_BLACK);
        y += 20;
    }

    renderer_display();
}

const PageVtbl page_jump_vtbl = {
    .id = PAGE_JUMP,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#endif // PC_SIMULATION
```

- [ ] **步骤 3：更新 Makefile 并注册页面**

在 `app/Makefile` 的 SRCS 列表中添加：
```
shared/ui/page_jump.c
```

在 `app/sim_main.c` 中添加：
```c
#include "page_jump.h"
// ...
page_mgr_register(page_jump_vtbl);
```

- [ ] **步骤 4：编译验证并 Commit**

```bash
cd app && make clean && make
git add -A
git commit -m "feat(pc-sim): add page_jump for page number input"
```

---

## 任务 6：补充缺失页面 — page_sleep

**文件：**
- 创建：`app/shared/ui/page_sleep.h`
- 创建：`app/shared/ui/page_sleep.c`
- 修改：`app/sim_main.c`
- 修改：`app/Makefile`

- [ ] **步骤 1：创建 page_sleep.h**

```c
// app/shared/ui/page_sleep.h
#ifndef PAGE_SLEEP_H
#define PAGE_SLEEP_H

#include "page_mgr.h"

extern const PageVtbl page_sleep_vtbl;

#endif // PAGE_SLEEP_H
```

- [ ] **步骤 2：创建 page_sleep.c**

```c
// app/shared/ui/page_sleep.c
#ifdef PC_SIMULATION

#include "page_sleep.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "mock_rtc.h"
#include "mock_weather.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define POEM_COUNT 10

static const char *poems[][3] = {
    {"静夜思", "李白", "床前明月光，\n疑是地上霜。\n举头望明月，\n低头思故乡。"},
    {"春晓", "孟浩然", "春眠不觉晓，\n处处闻啼鸟。\n夜来风雨声，\n花落知多少。"},
    {"登鹳雀楼", "王之涣", "白日依山尽，\n黄河入海流。\n欲穷千里目，\n更上一层楼。"},
    {"悯农", "李绅", "锄禾日当午，\n汗滴禾下土。\n谁知盘中餐，\n粒粒皆辛苦。"},
    {"相思", "王维", "红豆生南国，\n春来发几枝。\n愿君多采撷，\n此物最相思。"},
    {"江雪", "柳宗元", "千山鸟飞绝，\n万径人踪灭。\n孤舟蓑笠翁，\n独钓寒江雪。"},
    {"夜宿山寺", "李白", "危楼高百尺，\n手可摘星辰。\n不敢高声语，\n恐惊天上人。"},
    {"咏鹅", "骆宾王", "鹅鹅鹅，\n曲项向天歌。\n白毛浮绿水，\n红掌拨清波。"},
    {"风", "李峤", "解落三秋叶，\n能开二月花。\n过江千尺浪，\n入竹万竿斜。"},
    {"寻隐者不遇", "贾岛", "松下问童子，\n言师采药去。\n只在此山中，\n云深不知处。"},
};

static void render_clock_weather(void) {
    int cx = RENDERER_WIDTH / 2;
    struct tm t = mock_rtc_get_time();

    // 大字时间
    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", t.tm_hour, t.tm_min);
    int tw = renderer_text_width(time_buf);
    renderer_draw_text(cx - tw / 2, 60, time_buf, RENDERER_COLOR_BLACK);

    // 日期
    const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "星期%s %d月%d日", weekdays[t.tm_wday], t.tm_mon + 1, t.tm_mday);
    int dw = renderer_text_width(date_buf);
    renderer_draw_text(cx - dw / 2, 100, date_buf, RENDERER_COLOR_BLACK);

    // 天气
    const WeatherData *weather = mock_weather_get_current();
    if (weather) {
        int y = 140;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  %s", weather->city, weather->condition);
        int w = renderer_text_width(buf);
        renderer_draw_text(cx - w / 2, y, buf, RENDERER_COLOR_BLACK);

        snprintf(buf, sizeof(buf), "%d°C", weather->temp);
        w = renderer_text_width(buf);
        renderer_draw_text(cx - w / 2, y + 25, buf, RENDERER_COLOR_BLACK);
    }
}

static void render_poem(void) {
    struct tm t = mock_rtc_get_time();
    int idx = t.tm_min % POEM_COUNT;
    int cx = RENDERER_WIDTH / 2;

    // 诗名
    int tw = renderer_text_width(poems[idx][0]);
    renderer_draw_text(cx - tw / 2, 40, poems[idx][0], RENDERER_COLOR_BLACK);

    // 作者
    char author_buf[32];
    snprintf(author_buf, sizeof(author_buf), "——%s", poems[idx][1]);
    int aw = renderer_text_width(author_buf);
    renderer_draw_text(cx - aw / 2, 65, author_buf, RENDERER_COLOR_BLACK);

    // 诗句（逐行）
    const char *p = poems[idx][2];
    int y = 100;
    char line_buf[64];
    int line_idx = 0;
    while (*p) {
        if (*p == '\n') {
            line_buf[line_idx] = '\0';
            int lw = renderer_text_width(line_buf);
            renderer_draw_text(cx - lw / 2, y, line_buf, RENDERER_COLOR_BLACK);
            y += 22;
            line_idx = 0;
            p++;
        } else {
            line_buf[line_idx++] = *p++;
        }
    }
    if (line_idx > 0) {
        line_buf[line_idx] = '\0';
        int lw = renderer_text_width(line_buf);
        renderer_draw_text(cx - lw / 2, y, line_buf, RENDERER_COLOR_BLACK);
    }

    // 底部时间
    char time_buf[32];
    const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d  星期%s", t.tm_hour, t.tm_min, weekdays[t.tm_wday]);
    int tw2 = renderer_text_width(time_buf);
    renderer_draw_text(cx - tw2 / 2, RENDERER_HEIGHT - 30, time_buf, RENDERER_COLOR_BLACK);
}

static void on_enter(void) {
    // 无操作
}

static void on_key(KeyId key, KeyEvent event) {
    if (key == KEY_PWR && event == KEY_EVT_SHORT) {
        page_mgr_switch(PAGE_HOME);
    }
}

static void on_render(void) {
    renderer_clear(RENDERER_COLOR_WHITE);

    // 默认显示诗词布局
    render_poem();

    renderer_display();
}

const PageVtbl page_sleep_vtbl = {
    .id = PAGE_SLEEP,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#endif // PC_SIMULATION
```

- [ ] **步骤 3：更新 Makefile 并注册页面**

在 `app/Makefile` 的 SRCS 列表中添加：
```
shared/ui/page_sleep.c
```

在 `app/sim_main.c` 中添加：
```c
#include "page_sleep.h"
// ...
page_mgr_register(page_sleep_vtbl);
```

- [ ] **步骤 4：编译验证并 Commit**

```bash
cd app && make clean && make
git add -A
git commit -m "feat(pc-sim): add page_sleep with poem layout"
```

---

## 任务 7：增加长按支持

**文件：**
- 修改：`app/renderer_if.h`
- 修改：`app/renderer_sdl.c`
- 修改：`app/sim_main.c`

- [ ] **步骤 1：在 renderer_if.h 添加长按时间常量**

```c
// 在 renderer_if.h 中添加
#define RENDERER_LONG_PRESS_MS    800
#define RENDERER_SUPER_LONG_MS    2000
```

- [ ] **步骤 2：修改 renderer_sdl.c 实现长按检测**

在 `renderer_sdl.c` 的按键处理部分，添加长按计时逻辑：

```c
// 在文件顶部添加
static uint32_t s_key_down_time[RENDERER_KEY_COUNT] = {0};
static bool s_key_is_down[RENDERER_KEY_COUNT] = {false};

// 在 SDL_KEYDOWN 处理中添加
case SDLK_LEFT:
    if (!s_key_is_down[RENDERER_KEY_PREV]) {
        s_key_is_down[RENDERER_KEY_PREV] = true;
        s_key_down_time[RENDERER_KEY_PREV] = SDL_GetTicks();
    }
    break;

// 在 SDL_KEYUP 处理中添加
case SDLK_LEFT: {
    if (s_key_is_down[RENDERER_KEY_PREV]) {
        s_key_is_down[RENDERER_KEY_PREV] = false;
        uint32_t duration = SDL_GetTicks() - s_key_down_time[RENDERER_KEY_PREV];
        if (duration >= RENDERER_SUPER_LONG_MS) {
            s_callback(RENDERER_KEY_PREV, RENDERER_KEY_EVT_SUPER_LONG);
        } else if (duration >= RENDERER_LONG_PRESS_MS) {
            s_callback(RENDERER_KEY_PREV, RENDERER_KEY_EVT_LONG);
        } else {
            s_callback(RENDERER_KEY_PREV, RENDERER_KEY_EVT_SHORT);
        }
    }
    break;
}
```

对所有 4 个按键重复此逻辑。

- [ ] **步骤 3：在 sim_main.c 中处理长按事件**

修改 `on_key` 函数，添加长按处理：

```c
static void on_key(RendererKeyId key, RendererKeyEvent event) {
    // 映射到 PageKeyId
    KeyId page_key;
    switch (key) {
        case RENDERER_KEY_PWR:  page_key = KEY_PWR; break;
        case RENDERER_KEY_PREV: page_key = KEY_PREV; break;
        case RENDERER_KEY_NEXT: page_key = KEY_NEXT; break;
        case RENDERER_KEY_HOME: page_key = KEY_HOME; break;
        default: return;
    }

    // 映射事件
    KeyEvent page_event;
    switch (event) {
        case RENDERER_KEY_EVT_SHORT:       page_event = KEY_EVT_SHORT; break;
        case RENDERER_KEY_EVT_LONG:        page_event = KEY_EVT_LONG; break;
        case RENDERER_KEY_EVT_SUPER_LONG:  page_event = KEY_EVT_SUPER_LONG; break;
        case RENDERER_KEY_EVT_COMBO:       page_event = KEY_EVT_COMBO; break;
        default: return;
    }

    // 分发到页面状态机
    page_mgr_handle_key(page_key, page_event);
}
```

- [ ] **步骤 4：编译验证并 Commit**

```bash
cd app && make clean && make
git add -A
git commit -m "feat(pc-sim): add long press support for all keys"
```

---

## 任务 8：修复 sim_main.c 双轨问题

**文件：**
- 修改：`app/sim_main.c`

- [ ] **步骤 1：移除 custom_render 中的重复渲染逻辑**

删除 `custom_render()` 函数中的页面渲染代码，改为只调用页面状态机：

```c
static void custom_render(void) {
    // 只调用当前页面的 on_render
    const PageVtbl *page = page_mgr_get_currentVtbl();
    if (page && page->on_render) {
        page->on_render();
    }
    renderer_display();
}
```

- [ ] **步骤 2：移除 on_key 中的重复按键处理**

删除 `on_key()` 函数中的页面特定按键处理，改为只分发到页面状态机：

```c
static void on_key(RendererKeyId key, RendererKeyEvent event) {
    KeyId page_key;
    KeyEvent page_event;
    // ... 映射逻辑（同任务 7）...
    page_mgr_handle_key(page_key, page_event);
}
```

- [ ] **步骤 3：移除不再需要的全局状态变量**

删除以下全局变量：
- `s_current_menu`
- `s_in_reader`
- `s_in_bookshelf`
- `s_selected_book`
- `s_bookshelf_page`
- `s_reader_page`
- `s_book_text`
- `s_book_total_pages`
- `s_book_filename`
- `s_menu_visible`
- `s_menu_cursor`
- `s_show_info`

- [ ] **步骤 4：移除不再需要的辅助函数**

删除以下函数：
- `load_book_text()`
- `get_page_lines()`
- `draw_menu_bar()`

- [ ] **步骤 5：编译验证并 Commit**

```bash
cd app && make clean && make
git add -A
git commit -m "refactor(pc-sim): remove dual-track rendering, delegate to PageVtbl"
```

---

## 任务 9：集成测试

- [ ] **步骤 1：编译并运行仿真**

```bash
cd app && make clean && make && ./reader_sim
```

- [ ] **步骤 2：测试页面导航**

1. 主页 → 按右箭头 → 书架页
2. 书架页 → 按 Enter → 阅读页
3. 阅读页 → 按 Home → 菜单弹出
4. 菜单 → 按 Home → 继续阅读
5. 阅读页 → 按 ESC → 返回书架

- [ ] **步骤 3：测试长按功能**

1. 书架页 → 长按左箭头 → 翻页
2. 阅读页 → 长按右箭头 → 跳 5 页

- [ ] **步骤 4：测试排版引擎**

1. 打开一本 TXT 书籍
2. 验证分页正确
3. 修改字号设置
4. 验证重新分页

- [ ] **步骤 5：最终 Commit**

```bash
git add -A
git commit -m "test(pc-sim): verify all features working correctly"
```

---

## 检查清单

- [ ] 所有新文件已创建
- [ ] Makefile 已更新
- [ ] 编译通过（无警告）
- [ ] 页面导航正常
- [ ] 长按功能正常
- [ ] 排版引擎工作正常
- [ ] GBK 映射正确
- [ ] 无内存泄漏

---

**计划版本**：v1.0
**创建日期**：2026-06-16
**预计工作量**：9 个任务，约 2-3 小时

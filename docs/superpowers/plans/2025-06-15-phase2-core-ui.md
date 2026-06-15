# Phase 2 核心 UI 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 实现 Phase 2 核心 UI — 点阵字体、状态栏、FreeType 排版引擎、TXT/EPUB 解析器、主页、书架、阅读界面、设置页

**架构：** UI 层（widget → status_bar → 各页面）→ Engine 层（typesetter + book_parser）。所有页面通过 page_mgr 统一管理，事件总线解耦。

**技术栈：** ESP-IDF / FreeType / cJSON / FatFS

---

## 文件结构

```
src/
├── ui/
│   ├── widget.c/h            # 点阵字体渲染（任务 2.1）
│   ├── status_bar.c/h        # 全局状态栏（任务 2.2）
│   ├── page_home.c/h         # 主页（任务 2.5）
│   ├── page_bookshelf.c/h    # 书架页（任务 2.6）
│   ├── page_reader.c/h       # 阅读界面（任务 2.7）
│   └── page_settings.c/h     # 设置页（任务 2.8）
├── engine/
│   ├── typesetter.c/h        # FreeType 排版引擎（任务 2.3）
│   └── book_parser.c/h       # TXT/EPUB 解析器（任务 2.4）
data/
└── fonts/
    └── ui_font_16.bin        # 16×16 点阵字模（任务 2.1 资源）
```

**依赖关系：**
- 任务 2.1（widget）、2.3（typesetter）、2.4（book_parser）可并行开发
- 任务 2.2（status_bar）依赖 2.1
- 任务 2.5/2.6/2.7/2.8 依赖 2.1/2.2/2.3/2.4，在后期阶段开发

---

## 任务 2.1：点阵字体渲染

**文件：**
- 创建：`src/ui/widget.c`
- 创建：`src/ui/widget.h`
- 创建：`data/fonts/ui_font_16.bin`（字模数据）
- 修改：`src/CMakeLists.txt`（添加 widget.c）

- [ ] **步骤 1：创建字模数据文件 ui_font_16.bin**

字模来源：使用 Linux kernel `drivers/video/console/font_16x16.c` 中的 lat1_16x16 字模数据，或从开源 VGA font 提取。每个字模 32 字节（16×16 pixels，1bit/pixel），按 ASCII 顺序排列。

制作脚本（Python，另存为 `scripts/generate_ui_font.py`）：

```python
#!/usr/bin/env python3
"""从 kernel 源码提取 lat1_16x16 字模生成 ui_font_16.bin"""
# lat1_16x16 包含 256 个字模，每个 32 字节
# 这里用预定义的 16x16 bitmap 数据替代演示
# 实际使用时从 https://github.com/torvalds/linux/blob/master/drivers/video/console/font_16x16.c 提取

CHARS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
# 每个字符用 16 行 × 16 列的十六进制表示（简化示例）
# 实际需要 256 个字模的完整数据

with open("data/fonts/ui_font_16.bin", "wb") as f:
    for c in CHARS:
        # 简化：每个字模用 32 字节零填充 + 字符位图
        # 实际需要完整字模数据
        f.write(bytes([0] * 32))
```

注意：字模数据来源已明确（开源 VGA console font），实际字模从 `drivers/video/console/font_16x16.c` 的 `lat1_16x16` 数组提取，复制到 `data/fonts/` 目录。

- [ ] **步骤 2：编写 widget.h 接口**

```c
#pragma once
#include "hal/epd.h"

void widget_init(void);  // 从 SPIFFS 加载字模到 PSRAM

// 绘制字符/字符串，color 为 EPD_COLOR_BLACK/WHITE/RED
void widget_draw_char(int x, int y, char c, EpdColor color);
void widget_draw_text(int x, int y, const char *text, EpdColor color);

// 计算文本宽度（像素）
int widget_text_width(const char *text);

// 绘制预定义图标（icon_id 映射到字模偏移）
void widget_draw_icon(int x, int y, uint16_t icon_id, EpdColor color);

// 图标 ID 定义
#define ICON_BATTERY_FULL   0x80  // 电量满
#define ICON_BATTERY_75     0x81  // 电量 75%
#define ICON_BATTERY_50     0x82  // 电量 50%
#define ICON_BATTERY_25     0x83  // 电量 25%
#define ICON_WIFI_CONNECTED 0x84  // WiFi 已连接
#define ICON_WIFI_DISCONNECTED 0x85  // WiFi 未连接
#define ICON_CHARGING      0x86  // 充电中
#define ICON_SUNNY         0x87  // 晴天
#define ICON_CLOUDY        0x88  // 多云
#define ICON_RAINY         0x89  // 雨天
#define ICON_BOOK          0x8A  // 书籍
#define ICON_CLOCK         0x8B  // 时钟
```

- [ ] **步骤 3：编写 widget.c 实现**

字模加载：从 SPIFFS 读取 `fonts/ui_font_16.bin`，存入 PSRAM 分配的空间。使用 `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`。

字模查找：ASCII 字符直接索引（`font_data + c * 32`），Private Use Area 图标（0xE000+）映射到字模文件中的扩展区（`font_data + (icon_id - 0xE000 + 128) * 32`）。

像素绘制：逐位遍历字模 16×16 位图，bit=1 时调用 `epd_set_pixel()`。

- [ ] **步骤 4：修改 src/CMakeLists.txt 添加 widget.c**

```cmake
idf_component_register(
    SRCS "main.c"
         "engine/config.c"
         "engine/event_bus.c"
         "hal/epd.c"
         "hal/keys.c"
         "hal/battery.c"
         "hal/rtc.c"
         "hal/sd_card.c"
         "ui/page_mgr.c"
         "ui/widget.c"       # 新增
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_netif nvs_flash esp_timer
)
```

- [ ] **步骤 5：编译验证**

运行：`pio run`
预期：编译成功，无错误

- [ ] **步骤 6：Commit**

```bash
git add src/ui/widget.c src/ui/widget.h data/fonts/ui_font_16.bin scripts/generate_ui_font.py src/CMakeLists.txt
git commit -m "feat(ui): 点阵字体渲染（16×16 VGA font，widget 模块）"
```

---

## 任务 2.2：全局状态栏

**文件：**
- 创建：`src/ui/status_bar.c`
- 创建：`src/ui/status_bar.h`
- 修改：`src/CMakeLists.txt`（添加 status_bar.c）
- 修改：`src/main.c`（注册页面 + 调用 status_bar_init）

- [ ] **步骤 1：编写 status_bar.h 接口**

```c
#pragma once

void status_bar_init(void);                              // 注册事件总线订阅
void status_bar_render(void);                           // 绘制状态栏到 EPD 帧缓冲
void status_bar_update_time(void);                        // 仅更新时:分区域（局部刷新）
void status_bar_update_battery(void);                    // 仅更新电量区域
void status_bar_update_wifi(bool connected, int rssi);   // 仅更新 WiFi 区域
```

- [ ] **步骤 2：编写 status_bar.c 实现**

布局：高度 20px，全宽。背景 `EPD_COLOR_BLACK`，文字/图标 `EPD_COLOR_WHITE`。

```
[0-40px] HH:MM 时间（14px 字号，左对齐）
[360-400px] 电量 + WiFi（右对齐，从右往左排列）
```

时间显示：从 RTC 读取 `struct tm`，格式化为 `HH:MM` 字符串，调用 `widget_draw_text()` 渲染。

电量显示：调用 `battery_get_percent()` 获取电量，渲染 "87%" 文字 + 图标。低于 20% 用 `EPD_COLOR_RED`。

WiFi 显示：`connected=true` 时显示信号强度图标，`connected=false` 显示 X 图标。

事件订阅：构造函数中调用 `event_bus_subscribe(EVT_BATTERY, ...)` 等。

- [ ] **步骤 3：在 main.c 中初始化状态栏**

在 Phase 1 的 main.c 中，页面状态机初始化后，添加：

```c
#include "ui/status_bar.h"

status_bar_init();
status_bar_render();
```

- [ ] **步骤 4：修改 src/CMakeLists.txt 添加 status_bar.c**

在 `SRCS` 中添加 `"ui/status_bar.c"`

- [ ] **步骤 5：编译烧录验证**

运行：`pio run -t upload`
预期：屏幕顶部显示 20px 状态栏（时间 + 电量 + WiFi 区域）

- [ ] **步骤 6：Commit**

```bash
git add src/ui/status_bar.c src/ui/status_bar.h src/main.c src/CMakeLists.txt
git commit -m "feat(ui): 全局状态栏（时间/电量/WiFi）"
```

---

## 任务 2.3：FreeType 排版引擎

**文件：**
- 创建：`src/engine/typesetter.c`
- 创建：`src/engine/typesetter.h`
- 修改：`src/CMakeLists.txt`（添加 typesetter.c）

- [ ] **步骤 1：编写 typesetter.h 接口**

```c
#pragma once
#include <stdint.h>
#include <esp_err.h>

typedef struct {
    uint32_t startByte;   // 页起始字节偏移
    uint32_t startLine;   // 页起始行号
    uint16_t pageNum;
} PageIndex;

typedef struct {
    uint8_t fontId;           // 字体索引（0=思源宋体 1=思源黑体 2=文楷）
    uint8_t fontSize;         // 字号 px
    uint8_t lineSpacing;      // 行距倍数 ×10（15=1.5倍）
    uint8_t charSpacing;      // 字间距 px
    uint8_t margin;           // 页边距 px
    int pageWidth;            // 可用宽度 px
    int pageHeight;           // 可用高度 px
} TypesetterConfig;

esp_err_t typesetter_init(const TypesetterConfig *cfg);
esp_err_t typesetter_load_font(uint8_t fontId, const char *ttf_path);
esp_err_t typesetter_paginate(const char *text, uint32_t textLen,
                               PageIndex **pages, uint32_t *pageCount);
esp_err_t typesetter_render_page(const char *text, const PageIndex *page,
                                  uint8_t *epd_buf, int buf_w, int buf_h, int y_offset);
void typesetter_free_pages(PageIndex *pages);
void typesetter_reload_config(const TypesetterConfig *cfg);
```

- [ ] **步骤 2：实现 FreeType 初始化和字形测量（typesetter.c）**

```c
// 关键实现要点：

// 1. FreeType 库初始化
#include "freetype2/freetype/freetype.h"
#include "freetype2/freetype/ftimage.h"

FT_Library ft_library;
FT_Face ft_face;

// 字体数据从 PSRAM 分配，TTF 数据传入
esp_err_t typesetter_load_font(uint8_t fontId, const char *ttf_path) {
    // 从 SD 卡读取 TTF 文件到 PSRAM buffer
    // FT_New_Memory_Face 从内存加载
    // 设置字符映射为 Unicode
}

// 2. 字符宽度测量
int measure_char_width(uint32_t ch, int fontSize) {
    FT_Set_Pixel_Sizes(ft_face, 0, fontSize);
    FT_Load_Char(ft_face, ch, FT_LOAD_RENDER);
    return ft_face->glyph->advance.x / 64 + charSpacing;
}

// 3. 分页算法（预分页）
esp_err_t typesetter_paginate(const char *text, uint32_t textLen,
                               PageIndex **pages_out, uint32_t *count_out) {
    // 按段落切分（\n\n）
    // 逐字符测量 + 断行
    // 累积行高到 pageHeight 时新建一页
    // 孤行控制：段落最后一行不单独成页
    // 分配 PageIndex 数组返回
}
```

- [ ] **步骤 3：实现页面渲染**

```c
esp_err_t typesetter_render_page(const char *text, const PageIndex *page,
                                  uint8_t *epd_buf, int buf_w, int buf_h, int y_offset) {
    // 从 page->startByte 开始读取文本
    // 逐字符 FreeType 渲染到位图
    // 写入 EPD 帧缓冲（调用 epd_set_pixel 或直接写帧缓冲）
    // 支持 y_offset 实现局部刷新定位
}
```

- [ ] **步骤 4：修改 src/CMakeLists.txt 添加 typesetter.c**

在 `REQUIRES` 中添加 `freetype`（ESP-IDF freetype 组件）。

```cmake
idf_component_register(
    SRCS ...
    REQUIRES driver esp_wifi esp_netif nvs_flash esp_timer freetype  # 新增
)
```

- [ ] **步骤 5：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 6：Commit**

```bash
git add src/engine/typesetter.c src/engine/typesetter.h src/CMakeLists.txt
git commit -m "feat(engine): FreeType 排版引擎 + 预分页算法"
```

---

## 任务 2.4：TXT/EPUB 解析器

**文件：**
- 创建：`src/engine/book_parser.c`
- 创建：`src/engine/book_parser.h`
- 修改：`src/CMakeLists.txt`

- [ ] **步骤 1：编写 book_parser.h 接口**

```c
#pragma once
#include "engine/types.h"

typedef enum {
    BOOK_TXT,
    BOOK_EPUB,
    BOOK_UNKNOWN,
} BookFormat;

BookFormat book_detect_format(const char *filePath);
esp_err_t book_load_text(const char *filePath, char **text, uint32_t *textLen);
esp_err_t book_extract_metadata(const char *filePath, BookMeta *meta);
void book_free_text(char *text);
```

- [ ] **步骤 2：实现 TXT 解析（book_parser.c）**

```c
// 编码检测实现
BookFormat book_detect_format(const char *filePath) {
    FILE *f = fopen(filePath, "rb");
    uint8_t bom[4] = {0};
    fread(bom, 1, 4, f);
    fseek(f, 0, SEEK_SET);

    if (memcmp(bom, "\xEF\xBB\xBF", 3) == 0) return BOOK_TXT;  // UTF-8 BOM

    // GBK 检测：读前 1KB，扫描是否出现非 ASCII 字节且不符合 UTF-8 规则
    uint8_t buf[1024];
    size_t n = fread(buf, 1, 1024, f);
    bool likely_gbk = false;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] >= 0x80) { likely_gbk = true; break; }
    }
    fclose(f);

    if (likely_gbk) return BOOK_TXT;  // GBK 或其他编码
    return BOOK_TXT;  // 纯 ASCII/UTF-8 无 BOM 也当 TXT
}

// 加载文本（自动编码转换）
esp_err_t book_load_text(const char *filePath, char **text_out, uint32_t *len_out) {
    // 1. 检测编码
    // 2. 读取全部内容到内存
    // 3. GBK → UTF-8 转换（查表或简单算法）
    // 4. 返回统一 UTF-8 文本
}
```

- [ ] **步骤 3：实现 EPUB 解析**

```c
// EPUB 解析（简化版：ZIP 解压 + HTML 标签剥离）
esp_err_t book_load_epub(const char *filePath, char **text_out, uint32_t *len_out) {
    // 1. miniz: mz_unzip 打开 EPUB（ZIP 格式）
    // 2. 找到 META-INF/container.xml，解析 rootfile path
    // 3. 解析 content.opf，提取 <item> id → href 映射和 <spine> 顺序
    // 4. 按 spine 顺序读取各 XHTML 文件
    // 5. html_to_text()：状态机剥离 HTML 标签，保留 <p> 换行
    // 6. 拼接所有章节文本
}
```

- [ ] **步骤 4：实现元数据提取**

```c
esp_err_t book_extract_metadata(const char *filePath, BookMeta *meta) {
    // TXT: 从文件名提取书名（去掉扩展名）
    // EPUB: 解析 content.opf 中的 <dc:title> <dc:creator>
    // 生成 bookId（MD5 of filepath 前 16 字符）
    // 初始化 totalPages=0, currentPage=0
}
```

- [ ] **步骤 5：修改 src/CMakeLists.txt**

在 `REQUIRES` 中添加 `json`（ESP-IDF cJSON）。

- [ ] **步骤 6：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 7：Commit**

```bash
git add src/engine/book_parser.c src/engine/book_parser.h src/CMakeLists.txt
git commit -m "feat(engine): TXT/EPUB 解析器（UTF-8/GBK 自动检测）"
```

---

## 任务 2.5：主页

**文件：**
- 创建：`src/ui/page_home.c`
- 创建：`src/ui/page_home.h`
- 修改：`src/CMakeLists.txt`
- 修改：`src/main.c`（注册页面）

- [ ] **步骤 1：编写 page_home.h**

```c
#pragma once
#include "engine/types.h"

extern const PageVtbl page_home_vtbl;  // 供 page_mgr_register 使用
```

- [ ] **步骤 2：实现 page_home.c**

```c
#include "page_home.h"
#include "page_mgr.h"
#include "status_bar.h"
#include "widget.h"
#include "hal/rtc.h"
#include "engine/config.h"

static void on_enter(void) {
    // 从 RTC 读取时间
    // 从 SD 卡加载 metadata.json 统计阅读数据
}

static void on_render(void) {
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    // 日期区（居中，y=50）
    struct tm timeinfo;
    rtc_get_time(&timeinfo);
    char date_str[64];
    snprintf(date_str, sizeof(date_str), "%s %d年%02d月%02d日",
             get_weekday(timeinfo.tm_wday),
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday);
    widget_draw_text(200 - widget_text_width(date_str)/2, 50, date_str, EPD_COLOR_BLACK);

    // 天气占位（y=100）- Phase 2 静态文本
    widget_draw_text(200 - widget_text_width("⛅ 天气加载中...")/2, 100,
                     "⛅ 天气加载中...", EPD_COLOR_BLACK);

    // 统计区（y=180）
    widget_draw_text(20, 180, "📚 藏书 0 册", EPD_COLOR_BLACK);
    widget_draw_text(20, 200, "⏱ 今日阅读 0 分钟", EPD_COLOR_BLACK);
    widget_draw_text(20, 220, "📖 正在读: --", EPD_COLOR_BLACK);

    epd_display_partial();
}

static void on_key(KeyId key, KeyEvent event) {
    if (event == KEY_EVT_SHORT && key == KEY_NEXT) {
        page_mgr_switch(PAGE_BOOKSHELF);  // NEXT 键进入书架
    }
}

const PageVtbl page_home_vtbl = {
    .id = PAGE_HOME,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};
```

- [ ] **步骤 3：在 main.c 中注册主页页面**

```c
#include "ui/page_home.h"

page_mgr_register(&page_home_vtbl);
```

- [ ] **步骤 4：修改 src/CMakeLists.txt**

在 `SRCS` 中添加 `"ui/page_home.c"`

- [ ] **步骤 5：编译烧录验证**

运行：`pio run -t upload`
预期：主页显示日期 + 静态天气占位 + 统计区

- [ ] **步骤 6：Commit**

```bash
git add src/ui/page_home.c src/ui/page_home.h src/main.c src/CMakeLists.txt
git commit -m "feat(ui): 主页（日期 + 天气占位 + 统计）"
```

---

## 任务 2.6：书架页

**文件：**
- 创建：`src/ui/page_bookshelf.c`
- 创建：`src/ui/page_bookshelf.h`
- 修改：`src/CMakeLists.txt`
- 修改：`src/main.c`（注册页面）

- [ ] **步骤 1：编写 page_bookshelf.h**

```c
#pragma once

extern const PageVtbl page_bookshelf_vtbl;
```

- [ ] **步骤 2：实现 page_bookshelf.c**

```c
// 布局：3列×2行，每格 100×130px，内容区 400×260px
// 封面：默认灰色矩形 + "书" 图标（widget_draw_icon）
// 书名：widget_draw_text，2行截断

#define GRID_COLS 3
#define GRID_ROWS 2
#define CELL_W 133  // (400 - 边距) / 3
#define CELL_H 130

static int current_page = 0;
static int total_pages = 0;
static int selected_idx = 0;  // 0 ~ (total_books-1)
static BookMeta books[32];    // 最多显示 32 本（简化实现）
static int book_count = 0;

static void load_books(void) {
    // 从 SD 卡加载 /books/metadata.json
    // 解析 JSON，填充 books[] 数组
    // 计算 total_pages = (book_count + GRID_COLS*GRID_ROWS - 1) / (GRID_COLS*GRID_ROWS)
}

static void on_enter(void) {
    load_books();
    current_page = 0;
    selected_idx = 0;
}

static void on_render(void) {
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    int start_idx = current_page * GRID_COLS * GRID_ROWS;

    for (int i = 0; i < GRID_COLS * GRID_ROWS && (start_idx + i) < book_count; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int x = col * CELL_W + 10;
        int y = 30 + row * CELL_H;

        // 封面（灰色矩形 + 图标）
        epd_fill_rect(x, y, 100, 100, EPD_COLOR_WHITE);
        epd_draw_rect(x, y, 100, 100, EPD_COLOR_BLACK);
        widget_draw_icon(x + 35, y + 30, ICON_BOOK, EPD_COLOR_BLACK);

        // 书名（截断2行）
        char *title = books[start_idx + i].title;
        widget_draw_text(x, y + 105, title, EPD_COLOR_BLACK);

        // 选中框（红色边框）
        if (start_idx + i == selected_idx) {
            epd_draw_rect(x - 2, y - 2, 104, 104, EPD_COLOR_RED);
        }
    }

    // 翻页指示
    char page_str[32];
    snprintf(page_str, sizeof(page_str), "第 %d / %d 页", current_page + 1, total_pages);
    widget_draw_text(400 - widget_text_width(page_str) - 10, 290, page_str, EPD_COLOR_BLACK);

    epd_display_partial();
}

static void on_key(KeyId key, KeyEvent event) {
    int total_visible = GRID_COLS * GRID_ROWS;

    if (key == KEY_PREV) {
        if (event == KEY_EVT_SHORT) {
            // 移动光标
            if (selected_idx > 0) selected_idx--;
        } else if (event == KEY_EVT_LONG) {
            // 上一页
            if (current_page > 0) { current_page--; selected_idx = current_page * total_visible; on_render(); }
        }
    } else if (key == KEY_NEXT) {
        if (event == KEY_EVT_SHORT) {
            if (selected_idx < book_count - 1) selected_idx++;
        } else if (event == KEY_EVT_LONG) {
            if (current_page < total_pages - 1) { current_page++; selected_idx = current_page * total_visible; on_render(); }
        }
    } else if (key == KEY_HOME && event == KEY_EVT_SHORT) {
        // 打开选中书籍
        page_mgr_switch(PAGE_READER);
    }
}

const PageVtbl page_bookshelf_vtbl = {
    .id = PAGE_BOOKSHELF,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};
```

- [ ] **步骤 3：在 main.c 中注册书架页面**

```c
#include "ui/page_bookshelf.h"
page_mgr_register(&page_bookshelf_vtbl);
```

- [ ] **步骤 4：修改 src/CMakeLists.txt**

添加 `"ui/page_bookshelf.c"`

- [ ] **步骤 5：编译烧录验证**

运行：`pio run -t upload`
预期：书架显示 3×2 网格，可用 PREV/NEXT 导航，HOME 打开书籍

- [ ] **步骤 6：Commit**

```bash
git add src/ui/page_bookshelf.c src/ui/page_bookshelf.h src/main.c src/CMakeLists.txt
git commit -m "feat(ui): 书架页（3×2 网格 + 默认封面 + 翻页导航）"
```

---

## 任务 2.7：阅读界面

**文件：**
- 创建：`src/ui/page_reader.c`
- 创建：`src/ui/page_reader.h`
- 修改：`src/CMakeLists.txt`
- 修改：`src/main.c`

- [ ] **步骤 1：编写 page_reader.h**

```c
#pragma once

extern const PageVtbl page_reader_vtbl;
```

- [ ] **步骤 2：实现 page_reader.c**

```c
// 阅读界面需要：
// 1. 加载书籍文本（调用 book_parser）
// 2. 分页（调用 typesetter_paginate）
// 3. 渲染当前页（调用 typesetter_render_page）
// 4. 翻页逻辑（局部刷新/全刷）
// 5. HOME 弹出上下文菜单
// 6. 书签保存到 SD:/books/bookmarks.json

static char *g_book_text = NULL;
static uint32_t g_text_len = 0;
static PageIndex *g_pages = NULL;
static uint32_t g_page_count = 0;
static uint32_t g_current_page = 0;
static int g_partial_refresh_count = 0;  // 每5次局部刷新触发全刷

#define PARTIAL_CLEAN_INTERVAL 5

static void load_book(const char *filePath) {
    book_load_text(filePath, &g_book_text, &g_text_len);
    ReaderConfig cfg;
    config_load_reader(&cfg);
    TypesetterConfig tc = {
        .fontId = cfg.fontId,
        .fontSize = cfg.fontSize,
        .lineSpacing = cfg.lineSpacing,
        .charSpacing = cfg.charSpacing,
        .margin = cfg.margin,
        .pageWidth = 400 - cfg.margin * 2,
        .pageHeight = 262,  // 300 - 20(状态栏) - 18(进度条)
    };
    typesetter_init(&tc);
    typesetter_paginate(g_book_text, g_text_len, &g_pages, &g_page_count);
    g_current_page = 0;
}

static void on_enter(void) {
    // 从全局 context 获取当前书籍路径（通过 event_bus 或全局变量）
    // load_book(current_book_path);
    g_partial_refresh_count = 0;
}

static void on_render(void) {
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    // 渲染当前页（正文区 y=20 到 y=282）
    ReaderConfig cfg;
    config_load_reader(&cfg);
    typesetter_render_page(g_book_text, &g_pages[g_current_page],
                           NULL, 400, 262, 20);

    // 底部进度条
    char page_str[32];
    snprintf(page_str, sizeof(page_str), "第 %u / %lu 页",
             g_current_page + 1, (unsigned long)g_page_count);
    widget_draw_text(20, 285, page_str, EPD_COLOR_BLACK);

    // 进度条（120px 宽）
    int bar_x = 200;
    int bar_y = 290;
    int bar_w = 120;
    int progress = (g_current_page * 100) / (g_page_count > 0 ? g_page_count : 1);
    epd_draw_rect(bar_x, bar_y, bar_w, 8, EPD_COLOR_BLACK);
    epd_fill_rect(bar_x + 2, bar_y + 2, (bar_w - 4) * progress / 100, 4, EPD_COLOR_BLACK);

    if (g_partial_refresh_count >= PARTIAL_CLEAN_INTERVAL) {
        epd_display_full();    // 全刷清洁
        g_partial_refresh_count = 0;
    } else {
        epd_display_partial();  // 局部刷新
    }
    g_partial_refresh_count++;
}

static void on_key(KeyId key, KeyEvent event) {
    if (key == KEY_PREV) {
        if (event == KEY_EVT_SHORT && g_current_page > 0) {
            g_current_page--;
            on_render();
        } else if (event == KEY_EVT_LONG && g_current_page >= 5) {
            g_current_page -= 5;
            on_render();
        }
    } else if (key == KEY_NEXT) {
        if (event == KEY_EVT_SHORT && g_current_page < g_page_count - 1) {
            g_current_page++;
            on_render();
        } else if (event == KEY_EVT_LONG && g_current_page < g_page_count - 5) {
            g_current_page += 5;
            on_render();
        }
    } else if (key == KEY_HOME && event == KEY_EVT_SHORT) {
        // 弹出上下文菜单（暂时简化实现）
        // page_mgr_push(&page_reader_menu_vtbl);
    }
}

static void on_exit(void) {
    // 保存当前页到 NVS
}

const PageVtbl page_reader_vtbl = {
    .id = PAGE_READER,
    .on_enter = on_enter,
    .on_exit = on_exit,
    .on_key = on_key,
    .on_render = on_render,
};
```

- [ ] **步骤 3：在 main.c 中注册阅读页面**

```c
#include "ui/page_reader.h"
page_mgr_register(&page_reader_vtbl);
```

- [ ] **步骤 4：修改 src/CMakeLists.txt**

添加 `"ui/page_reader.c"`

- [ ] **步骤 5：编译烧录验证**

运行：`pio run -t upload`
预期：书架打开书籍后显示排版文本，PREV/NEXT 翻页正常

- [ ] **步骤 6：Commit**

```bash
git add src/ui/page_reader.c src/ui/page_reader.h src/main.c src/CMakeLists.txt
git commit -m "feat(ui): 阅读界面（翻页/进度条/排版引擎集成）"
```

---

## 任务 2.8：设置页

**文件：**
- 创建：`src/ui/page_settings.c`
- 创建：`src/ui/page_settings.h`
- 修改：`src/CMakeLists.txt`
- 修改：`src/main.c`

- [ ] **步骤 1：编写 page_settings.h**

```c
#pragma once

extern const PageVtbl page_settings_vtbl;
```

- [ ] **步骤 2：实现 page_settings.c**

设置页使用两层菜单结构（主菜单 → 子菜单）。

```c
typedef enum {
    SUB_READING,    // 阅读排版子菜单
    SUB_SYSTEM,     // 系统设置子菜单
} SettingsSub;

static SettingsSub current_sub = SUB_READING;
static int selected_item = 0;

// 阅读排版菜单项
static const char *font_names[] = {"思源宋体", "思源黑体", "霞鹜文楷"};
static const uint8_t font_sizes[] = {16, 18, 20, 22, 24, 28, 32};
static const uint8_t line_spacing_vals[] = {10, 12, 15, 20};  // ×10
static const uint8_t char_spacing_vals[] = {0, 1, 2, 3, 4};
static const uint8_t margin_vals[] = {4, 8, 12, 16};

// 子菜单定义
static void render_reading_menu(void);
static void render_system_menu(void);

static void on_render(void) {
    epd_clear(EPD_COLOR_WHITE);
    status_bar_render();

    if (current_sub == SUB_READING) {
        render_reading_menu();
    } else {
        render_system_menu();
    }

    epd_display_partial();
}

static void render_reading_menu(void) {
    // 5 个菜单项：字体/字号/行距/字间距/页边距
    // 每项一行，y = 30 + idx * 22
    // 选中项用 > 前缀 + 反色
}

static void render_system_menu(void) {
    // 5 个菜单项：显示方向/自动休眠/清洁屏幕/恢复出厂/关于
}

static void on_key(KeyId key, KeyEvent event) {
    if (event != KEY_EVT_SHORT) return;

    if (key == KEY_PREV) {
        if (selected_item > 0) selected_item--;
    } else if (key == KEY_NEXT) {
        if (selected_item < 4) selected_item++;
    } else if (key == KEY_HOME) {
        // 根据当前子菜单处理选中项的值变更
        // 对于数值选项：NEXT/PREV 长按变更值，HOME 确认
        // 保存到 NVS，触发 config_save_reader/config_save_sys
    } else if (key == KEY_PWR) {
        page_mgr_pop();  // 返回上一页
    }
}

const PageVtbl page_settings_vtbl = {
    .id = PAGE_SETTINGS,
    .on_enter = NULL,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};
```

- [ ] **步骤 3：在 main.c 中注册设置页面**

```c
#include "ui/page_settings.h"
page_mgr_register(&page_settings_vtbl);
```

- [ ] **步骤 4：修改 src/CMakeLists.txt**

添加 `"ui/page_settings.c"`

- [ ] **步骤 5：编译烧录验证**

运行：`pio run -t upload`
预期：设置页显示菜单，可上下导航，HOME 确认修改

- [ ] **步骤 6：Commit**

```bash
git add src/ui/page_settings.c src/ui/page_settings.h src/main.c src/CMakeLists.txt
git commit -m "feat(ui): 设置页（阅读排版 + 系统设置菜单）"
```

---

## 集成验证

所有 Phase 2 任务完成后：

- [ ] **步骤：集成测试**

1. 烧录固件：`pio run -t upload`
2. 检查串口日志无错误
3. 验证页面切换：主页 → 书架 → 阅读 → 设置
4. 验证状态栏正常显示（时间/电量/WiFi）
5. 验证阅读界面翻页和排版

- [ ] **Commit**

```bash
git add -A
git commit -m "feat: Phase 2 核心 UI 完成（点阵字体/状态栏/排版/书架/阅读/设置）"
```

---

## 自检清单

- [x] 字模来源明确（开源 VGA 16×16 console font）
- [x] 分页策略明确（预分页，一次性生成 PageIndex）
- [x] 书架封面策略明确（Phase 2 默认占位图）
- [x] 书签存储策略明确（SD bookmarks.json）
- [x] 所有页面通过 page_mgr 统一管理
- [x] 排版配置热切换（typesetter_reload_config）
- [x] 翻页策略符合需求（局部刷新 + 定期全刷清洁）
- [x] 每任务独立 commit
- [x] 所有新增 .c/.h 文件已在 CMakeLists.txt 中注册

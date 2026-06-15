# Phase 2 核心 UI 设计规格

**日期：** 2025-06-15
**阶段：** Phase 2 · 核心 UI
**状态：** 已批准

---

## 1. 概述

Phase 2 实现核心 UI 层和排版引擎，包含 8 个任务：
- 2.1 点阵字体渲染
- 2.2 全局状态栏
- 2.3 FreeType 排版引擎 + 分页算法
- 2.4 TXT/EPUB 解析器
- 2.5 主页
- 2.6 书架页
- 2.7 阅读界面
- 2.8 设置页

---

## 2. 技术决策

| 模块 | 方案 | 理由 |
|------|------|------|
| 点阵字体 | B - 开源 VGA console font（16×16） | 状态栏字符集小，体积极小（~3KB），开源成熟 |
| 分页策略 | A - 预分页 | 翻页流畅体验优先，PSRAM 充足 |
| 书架封面 | A - 默认占位图先行 | Phase 2 简化实现，后续 Phase 3 对接真实封面 |
| 书签存储 | B - SD 卡 `bookmarks.json` | 扩展性好，支持多书签和多书籍 |

---

## 3. 模块设计

### 3.1 点阵字体（任务 2.1）

**文件：** `main/ui/widget.c/h`

**字模来源：** 使用开源 VGA 16×16 console font（如 Linux kernel `drivers/video/console/font_16x16.c`），或 Peter Miller 的 vga16-font。每个字模 32 字节（256 像素，1bit/pixel）。

**字符集范围：** 仅实现状态栏所需的 ASCII 子集：
- 数字 0-9
- 大写字母 A-Z（用于 AM/PM）
- 符号：`:` `.` `%` `+` `V` `×` `✓` `✕` `📶` `🔋`（用 Unicode Private Use Area 映射到字模索引）

**接口：**

```c
// widget.h
void widget_init(void);                                        // 从 SPIFFS 加载字模到 PSRAM
void widget_draw_char(int x, int y, char c, EpdColor color);   // 绘制单个字符
void widget_draw_text(int x, int y, const char *text, EpdColor color);  // 绘制字符串
int widget_text_width(const char *text);                       // 计算字符串宽度（像素）
void widget_draw_icon(int x, int y, uint16_t icon_id, EpdColor color);  // 绘制图标
```

**资源文件：** `data/fonts/ui_font_16.bin` — 预生成的字模二进制，烧录到 SPIFFS 分区。

### 3.2 全局状态栏（任务 2.2）

**文件：** `main/ui/status_bar.c/h`

**布局：** 高度 20px，全宽，深色背景（黑底白字）。

```
┌──────────────────────────────────────────────────┐
│  14:35                            🔋87%  📶      │
│  ←时间（左）              电量+WiFi（右，从右）    │
└──────────────────────────────────────────────────┘
```

**刷新策略：**
- 时间区域：每分钟刷新（局部刷新）
- 电量区域：每 5% 变化时刷新，或充电时每分钟刷新
- WiFi 区域：连接/断开时刷新
- 三色变化（如电量 < 20% 变红）：合并到下一次全刷

**接口：**

```c
// status_bar.h
void status_bar_init(void);                              // 注册到事件总线
void status_bar_render(void);                            // 绘制到 EPD 帧缓冲（黑底白字）
void status_bar_update_time(void);                        // 仅更新时分钟区域（局部刷新）
void status_bar_update_battery(void);                     // 仅更新电量区域
void status_bar_update_wifi(bool connected, int rssi);   // 仅更新 WiFi 区域
```

**订阅事件：** 订阅 `EVT_KEY`（唤醒时更新时间）、`EVT_BATTERY`（电量变化）、`EVT_WIFI_STATUS`（连接状态变化）。

### 3.3 FreeType 排版引擎（任务 2.3）

**文件：** `main/engine/typesetter.c/h`

**分页策略：** 预分页（A 方案）— 打开书籍时一次性扫描全文生成分页索引。

**分页算法：**

```
输入：原始文本、字号、字间距、行间距、页面宽高、页边距

1. 按段落切分文本（\n\n 或空行）
2. 对每段：
   a. 按字符逐一测量宽度（FreeType_Get_Char_Width）
   b. 遇到换行触发点（标点、空格、CJK 字符边界）决定断行
   c. 累积行高，当超过页面可用高度时，当前段落整体移至下一页
3. 生成 PageIndex 数组：[{startByte, startLine, pageNum}]
4. 孤行控制：段落最后一行不单独成页（如果只有 1 行，合并到上一页）
```

**接口：**

```c
// typesetter.h

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
```

**字体加载策略：** TTF 文件从 SD 卡 `/fonts/` 目录加载到 PSRAM，支持按需加载（首次打开某本书时加载对应字体）。

### 3.4 TXT/EPUB 解析器（任务 2.4）

**文件：** `main/engine/book_parser.c/h`

**TXT 编码检测：**

```
1. 检查 BOM（UTF-8 BOM: EF BB BF，UTF-16 LE: FF FE）
2. 若无 BOM，扫描前 1KB 字节：
   - 出现连续 ASCII 区间（0x20-0x7E）为主 → UTF-8 或 ASCII
   - 出现 0x80-0xFF 字节流且不符合 UTF-8 规则 → GBK
3. GBK → UTF-8 内存转换（用于统一处理）
```

**EPUB 解析：**

```
1. ZIP 解压（使用 miniz 或内置 zlib）
2. 解析 container.xml 找到 content.opf 路径
3. 从 content.opf 提取 <item> 列表，筛选 media-type="application/xhtml+xml"
4. 按 spine 顺序拼接各 XHTML 文件
5. HTML 标签剥离（正则或状态机），保留 <p> <br> 换行
```

**元数据提取：** TXT 从文件名或文件头注释提取；EPUB 从 content.opf 的 `<dc:title>` `<dc:creator>` 提取。

**接口：**

```c
// book_parser.h

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

### 3.5 主页（任务 2.5）

**文件：** `main/ui/page_home.c/h`

**布局（内容区 400×260px）：**

```
┌──────────────────────────────────┐
│  状态栏（20px）                   │
├──────────────────────────────────┤
│     星期三  2025年06月15日         │  ← 日期区（居中，18px）
├──────────────────────────────────┤
│     ⛅ 北京                       │
│     晴转多云  26°C / 18°C        │  ← 天气区（占位，Phase 3 接入真实数据）
│     空气质量:良  湿度:55%         │
├──────────────────────────────────┤
│     📚 藏书 42 册                  │
│     ⏱ 今日阅读 38 分钟           │  ← 统计区（从 metadata.json 计算）
│     📖 正在读:《三体》            │
└──────────────────────────────────┘
```

**天气占位：** Phase 2 使用静态占位文本，图标用 widget 图标代替。Phase 3 接入和风天气 API 后替换为动态数据。

### 3.6 书架页（任务 2.6）

**文件：** `main/ui/page_bookshelf.c/h`

**布局：** 3 列 × 2 行，每格 100×130px，内容区 400×260px。

```
┌────────┬────────┬────────┐
│ [封面] │ [封面] │ [封面] │
│ 书名A  │ 书名B  │ 书名C  │
├────────┼────────┼────────┤
│ [封面] │ [选中] │ [封面] │
│ 书名D  │ 书名E  │ 书名F  │
└────────┴────────┴────────┘
                   第 1/3 页
```

**封面策略：** Phase 2 使用默认占位图（灰色矩形 + 书脊图标）。封面从 `SD:/books/covers/` 加载的功能在 Phase 3 实现。

**导航：**
- PREV/NEXT 短按：移动光标（按行/列顺序）
- PREV/NEXT 长按：翻页
- HOME 短按：打开选中书籍 → 切换到 PAGE_READER

**选中状态：** 红色边框（2px），全刷时更新（不单独做局部刷新）。

**底部翻页指示：** `第 N / 共 M 页`，右对齐，14px 字号。

### 3.7 阅读界面（任务 2.7）

**文件：** `main/ui/page_reader.c/h`

**布局：**

```
┌──────────────────────────────────┐
│ 第三章 深渊中的黑暗森林   [电量][wifi] │ ← 章节标题栏（20px）
├──────────────────────────────────┤
│                                   │
│  正文内容区（排版引擎渲染）         │ ← 正文区 262px
│  支持 TXT/EPUB                    │
│                                   │
├──────────────────────────────────┤
│  第 23 页 / 共 312 页   ████░░ 7% │ ← 进度条 + 页码
└──────────────────────────────────┘
```

**翻页策略：**
- PREV/NEXT 短按：翻 1 页（黑白局部刷新 ~250ms）
- PREV/NEXT 长按：翻 5 页
- 每 5 次局部刷新后触发一次黑白全刷清洁（~2s，防残影）
- 计数持久化到 NVS

**HOME 上下文菜单：**

```
┌────────────────────────┐
│  ▶ 继续阅读             │ ← 默认选中
│    跳转到...            │ ← 弹出数字输入
│    添加书签             │
│    书籍信息             │
└────────────────────────┘
```

菜单使用 `page_mgr_push()` 入栈，ESC 或非菜单项按键 `page_mgr_pop()` 出栈。

**书签存储：** `SD:/books/bookmarks.json`

```json
{
  "bookmarks": [
    {
      "bookId": "abc123",
      "pageNum": 42,
      "label": "第三章精彩段落",
      "createdAt": 1715000000
    }
  ]
}
```

### 3.8 设置页（任务 2.8）

**文件：** `main/ui/page_settings.c/h`

**布局：** 两个子菜单，通过 NEXT/PREV 在条目间移动，HOME 确认/进入子菜单。

**阅读排版子菜单：**

| 条目 | 可选值 | 默认 |
|------|--------|------|
| 字体 | 思源宋体 / 思源黑体 / 霞鹜文楷 | 思源宋体 |
| 字号 | 16 / 18 / 20 / 22 / 24 / 28 / 32 px | 20px |
| 行间距 | 1.0 / 1.2 / 1.5 / 2.0 | 1.5 |
| 字间距 | 0 / 1 / 2 / 3 / 4 px | 1px |
| 页边距 | 4 / 8 / 12 / 16 px | 8px |

**系统设置子菜单：**

| 条目 | 说明 |
|------|------|
| 显示方向 | 0° / 90° / 180° / 270°，立即生效并重绘 |
| 自动休眠 | 5 / 10 / 30 分钟 / 不自动 |
| 清洁屏幕 | 手动执行全刷清洁 |
| 恢复出厂 | 需长按 HOME 3 秒确认 |
| 关于 | 固件版本 · 编译日期 · MAC 地址 |

**导航逻辑：**
- PREV/NEXT 短按：上/下移一个条目
- PREV/NEXT 长按（数值选项）：增加/减小当前值
- HOME 短按：确认当前选项 / 进入子菜单
- HOME 长按（恢复出厂）：3 秒确认倒计时

---

## 4. 页面栈集成

所有页面通过 `page_mgr`（Phase 1 任务 1.10）管理：

```c
// 页面注册（app_main 中）
page_mgr_register(&(const PageVtbl){
    .id = PAGE_HOME,
    .on_enter = page_home_on_enter,
    .on_exit = page_home_on_exit,
    .on_key = page_home_on_key,
    .on_render = page_home_on_render,
});
// ... 其他页面同理
```

**初始状态：** `page_mgr_switch(PAGE_HOME)` — Phase 1 Boot 页面完成后切换到 HOME。

---

## 5. 事件订阅

Phase 2 页面订阅的事件：

| 页面 | 订阅事件 | 处理 |
|------|----------|------|
| status_bar | EVT_BATTERY, EVT_WIFI_STATUS, EVT_KEY | 更新对应区域 |
| page_home | EVT_CONFIG_CHANGED | 刷新统计数据 |
| page_bookshelf | EVT_KEY | 导航和翻页 |
| page_reader | EVT_KEY | 翻页和菜单 |
| page_settings | EVT_KEY | 菜单导航 |

---

## 6. 文件结构

```
main/
├── ui/
│   ├── widget.c/h            # 点阵字体渲染（任务 2.1）
│   ├── status_bar.c/h        # 全局状态栏（任务 2.2）
│   ├── page_home.c/h         # 主页（任务 2.5）
│   ├── page_bookshelf.c/h    # 书架页（任务 2.6）
│   ├── page_reader.c/h       # 阅读界面（任务 2.7）
│   └── page_settings.c/h     # 设置页（任务 2.8）
├── engine/
│   ├── typesetter.c/h        # FreeType 排版引擎（任务 2.3）
│   └── book_parser.c/h      # TXT/EPUB 解析器（任务 2.4）
└── resources/
    └── fonts/
        └── ui_font_16.bin   # 16×16 点阵字模

data/
└── fonts/
    └── ui_font_16.bin       # SPIFFS 烧录版本
```

---

## 7. 依赖关系

```
任务 2.1（点阵字体）
    ↑
任务 2.2（状态栏） ──────→ 依赖 2.1
    │
任务 2.3（排版引擎） ────→ 独立，可并行
    │
任务 2.4（解析器） ──────→ 独立，可并行
    │
    ├──→ 任务 2.5（主页） ────→ 依赖 2.1, 2.2, 2.4
    ├──→ 任务 2.6（书架） ────→ 依赖 2.1, 2.2, 2.4
    ├──→ 任务 2.7（阅读） ────→ 依赖 2.1, 2.2, 2.3, 2.4
    └──→ 任务 2.8（设置） ────→ 依赖 2.1, 2.2

建议并行顺序：
  - (2.1 + 2.3 + 2.4) 可并行开发
  - 2.2 依赖 2.1，完成后开发
  - 2.5/2.6/2.7/2.8 依赖 2.1/2.2/2.3/2.4，最后阶段开发
```

---

## 8. 自检清单

- [x] 点阵字体方案已定（VGA 16×16 console font）
- [x] 分页策略已定（预分页）
- [x] 书架封面策略已定（占位图先行）
- [x] 书签存储已定（SD bookmarks.json）
- [x] 所有页面通过 page_mgr 统一管理
- [x] 排版配置热切换不需要重启阅读器
- [x] 翻页策略符合需求（三色全刷限制）
- [x] 任务间依赖关系清晰

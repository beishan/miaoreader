# PC 仿真改进反向移植到 ESP32 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将 PC 仿真中实现得更好的界面细节反向移植到 ESP32 固件，包括进度条、UTF-8 截断、双模式编辑、书籍信息弹窗等。

**架构：** 逐文件修改 ESP32 的 UI 页面代码，从 PC 仿真版本移植更好的实现逻辑，同时保持 ESP32 特有的功能（typesetter 引擎、NVS 持久化、EPD 刷新等）。

**技术栈：** ESP-IDF、FreeType、EPD 驱动、NVS

---

## 文件结构

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `src/ui/page_home.c` | 添加进度条、修复 UTF-8 截断、添加布局常量、添加分割线 |
| `src/ui/page_reader.c` | 添加进度条、修复 pageHeight 计算 |
| `src/ui/page_bookshelf.c` | 修复 UTF-8 书名截断、添加省略号 |
| `src/ui/page_settings.c` | 添加 BROWSE/EDIT 双模式、edit_value 缓冲 |
| `src/ui/page_menu.c` | 实现书籍信息弹窗、添加书签反馈 |
| `src/ui/page_jump.c` | 修复跳转页码传递 bug、操作提示居中 |
| `src/ui/page_reader.h` | 添加 page_reader_set_page 接口 |

---

## 任务 1：修复 UTF-8 书名截断（page_bookshelf）

**优先级：** P0

**文件：**
- 修改：`src/ui/page_bookshelf.c`

- [ ] **步骤 1：添加 UTF-8 感知的截断函数**

在 `src/ui/page_bookshelf.c` 顶部添加辅助函数：

```c
// UTF-8 感知的书名截断，最多 max_chars 个字符，超出添加 ".."
static void truncate_utf8(const char *src, char *dst, int max_chars, int dst_size) {
    int chars = 0;
    int i = 0;
    int last_good = 0;

    while (src[i] != '\0' && chars < max_chars) {
        last_good = i;
        if ((src[i] & 0x80) == 0) {
            // ASCII
            i++;
        } else if ((src[i] & 0xE0) == 0xC0) {
            i += 2;
        } else if ((src[i] & 0xF0) == 0xE0) {
            i += 3;
        } else if ((src[i] & 0xF8) == 0xF0) {
            i += 4;
        } else {
            i++; // 无效字节，跳过
        }
        chars++;
    }

    // 复制截断后的内容
    int copy_len = last_good;
    if (copy_len >= dst_size - 3) copy_len = dst_size - 4;
    memcpy(dst, src, copy_len);

    // 如果需要截断，添加省略号
    if (src[i] != '\0') {
        dst[copy_len] = '.';
        dst[copy_len + 1] = '.';
        dst[copy_len + 2] = '\0';
    } else {
        dst[copy_len] = '\0';
    }
}
```

- [ ] **步骤 2：修改书名显示逻辑**

找到书名截断代码（约行 100-102），替换为：

```c
// 旧代码：
// char title[12];
// strncpy(title, s_books[i].title, sizeof(title) - 1);
// title[sizeof(title) - 1] = '\0';

// 新代码：
char title[20];
truncate_utf8(s_books[i].title, title, 6, sizeof(title));
```

- [ ] **步骤 3：编译验证**

```bash
pio run
```

预期：编译通过

- [ ] **步骤 4：Commit**

```bash
git add src/ui/page_bookshelf.c
git commit -m "fix(epd): UTF-8 safe book title truncation in bookshelf"
```

---

## 任务 2：修复 UTF-8 书名截断（page_home）

**优先级：** P0

**文件：**
- 修改：`src/ui/page_home.c`

- [ ] **步骤 1：添加 UTF-8 截断函数**

在 `src/ui/page_home.c` 顶部添加与 page_bookshelf 相同的 `truncate_utf8` 函数。

- [ ] **步骤 2：修改书名截断逻辑**

找到书名截断代码（约行 106-111），替换为：

```c
// 旧代码：
// if (strlen(book_str) > 28) {
//     book_str[25] = '.';
//     book_str[26] = '.';
//     book_str[27] = '.';
//     book_str[28] = '\0';
// }

// 新代码：
if (widget_text_width(book_str) > EPD_WIDTH - 40) {
    char truncated[64];
    truncate_utf8(book_str + 12, truncated, 8, sizeof(truncated)); // +12 跳过 "正在读: "
    snprintf(book_str, sizeof(book_str), "正在读: %s..", truncated);
}
```

- [ ] **步骤 3：添加布局常量**

在文件顶部添加布局常量：

```c
// 布局常量
#define DATE_Y          40
#define WEATHER_Y       70
#define WEATHER_LINE_H  20
#define DIVIDER_Y       130
#define STATS_Y         150
#define STATS_LINE_H    20
#define PROGRESS_Y      210
#define FOOTER_Y        270
```

然后替换所有硬编码的 Y 坐标。

- [ ] **步骤 4：添加进度条**

在统计区之后、底部提示之前添加进度条：

```c
// 进度条
if (current_book) {
    uint32_t currentPage = current_book->currentPage;
    uint32_t totalPages = current_book->totalPages;
    if (totalPages > 0) {
        int percent = (int)((currentPage * 100UL) / totalPages);
        int bar_x = 20;
        int bar_w = EPD_WIDTH - 40;
        int bar_h = 8;
        int fill_w = (bar_w * percent) / 100;

        epd_draw_rect(bar_x, PROGRESS_Y, bar_w, bar_h, EPD_COLOR_BLACK);
        if (fill_w > 0) {
            epd_fill_rect(bar_x + 1, PROGRESS_Y + 1, fill_w - 2, bar_h - 2, EPD_COLOR_BLACK);
        }
    }
}
```

- [ ] **步骤 5：编译验证并 Commit**

```bash
pio run
git add src/ui/page_home.c
git commit -m "feat(epd): add progress bar and UTF-8 truncation to home page"
```

---

## 任务 3：添加阅读器进度条（page_reader）

**优先级：** P1

**文件：**
- 修改：`src/ui/page_reader.c`

- [ ] **步骤 1：调整布局常量**

修改布局常量，为进度条留出空间：

```c
#define CONTENT_Y  24
#define CONTENT_H  248   // 原 256，减少 8px 给进度条
#define PROGRESS_Y 278   // 进度条 Y 坐标
#define PROGRESS_H 8     // 进度条高度
```

- [ ] **步骤 2：添加进度条绘制**

在 `on_render` 函数中，页码文字之前添加进度条：

```c
// 进度条
if (s_page_count > 0) {
    int percent = (int)((s_current_page * 100UL) / s_page_count);
    int bar_x = 20;
    int bar_w = EPD_WIDTH - 40;
    int fill_w = (bar_w * percent) / 100;

    epd_draw_rect(bar_x, PROGRESS_Y, bar_w, PROGRESS_H, EPD_COLOR_BLACK);
    if (fill_w > 2) {
        epd_fill_rect(bar_x + 1, PROGRESS_Y + 1, fill_w - 2, PROGRESS_H - 2, EPD_COLOR_BLACK);
    }
}
```

- [ ] **步骤 3：修复 pageHeight 计算**

在 settings 中调用 typesetter 时，确保 pageHeight 与 CONTENT_H 一致：

```c
// 确保 pageHeight = CONTENT_H - margin * 2
```

- [ ] **步骤 4：编译验证并 Commit**

```bash
pio run
git add src/ui/page_reader.c
git commit -m "feat(epd): add progress bar to reader page"
```

---

## 任务 4：实现书籍信息弹窗（page_menu）

**优先级：** P0

**文件：**
- 修改：`src/ui/page_menu.c`
- 修改：`src/ui/page_menu.h`

- [ ] **步骤 1：扩展书籍信息传递**

修改 `page_menu_set_book_info` 函数，接收更多信息：

```c
// 在 page_menu.h 中添加
void page_menu_set_book_info_full(const char *book_name, uint32_t current_page,
                                   uint32_t total_pages, const char *file_path,
                                   uint32_t file_size, int bookmark_count);
```

- [ ] **步骤 2：添加书籍信息变量**

在 `page_menu.c` 中添加：

```c
static char s_file_path[256] = {0};
static uint32_t s_file_size = 0;
static int s_bookmark_count = 0;
```

- [ ] **步骤 3：实现书籍信息渲染**

添加书籍信息弹窗渲染函数：

```c
static void render_book_info(void) {
    renderer_clear(RENDERER_COLOR_WHITE);
    status_bar_render();

    int cx = EPD_WIDTH / 2;
    int y = 30;

    // 标题
    widget_draw_text(cx - widget_text_width("书籍信息") / 2, y, "书籍信息");
    y += 25;
    epd_draw_rect(20, y, EPD_WIDTH - 40, 1, EPD_COLOR_BLACK);
    y += 15;

    // 书名
    char buf[64];
    snprintf(buf, sizeof(buf), "书名: %s", s_book_name);
    widget_draw_text(20, y, buf);
    y += 20;

    // 页码
    snprintf(buf, sizeof(buf), "页码: %u / %u", s_current_page + 1, s_total_pages);
    widget_draw_text(20, y, buf);
    y += 20;

    // 进度
    if (s_total_pages > 0) {
        int percent = (int)((s_current_page * 100UL) / s_total_pages);
        snprintf(buf, sizeof(buf), "进度: %d%%", percent);
    } else {
        snprintf(buf, sizeof(buf), "进度: --");
    }
    widget_draw_text(20, y, buf);
    y += 20;

    // 书签数量
    snprintf(buf, sizeof(buf), "书签: %d 个", s_bookmark_count);
    widget_draw_text(20, y, buf);
    y += 30;

    // 返回提示
    widget_draw_text(cx - widget_text_width("按任意键返回") / 2, y, "按任意键返回");

    epd_display_full();
}
```

- [ ] **步骤 4：修改书籍信息按键处理**

修改 `case 3`（书籍信息）的处理：

```c
case 3: // 书籍信息
    render_book_info();
    // 等待任意按键
    s_showing_info = true;
    break;
```

添加 `s_showing_info` 状态变量，在 `on_key` 中处理。

- [ ] **步骤 5：编译验证并 Commit**

```bash
pio run
git add src/ui/page_menu.c src/ui/page_menu.h
git commit -m "feat(epd): implement book info popup in menu"
```

---

## 任务 5：添加书签反馈（page_menu）

**优先级：** P2

**文件：**
- 修改：`src/ui/page_menu.c`

- [ ] **步骤 1：添加反馈渲染函数**

```c
static void render_bookmark_added(void) {
    int cx = EPD_WIDTH / 2;
    int cy = EPD_HEIGHT / 2;

    // 绘制确认框
    epd_fill_rect(cx - 60, cy - 15, 120, 30, EPD_COLOR_BLACK);
    widget_draw_text(cx - widget_text_width("书签已添加") / 2, cy - 8, "书签已添加");

    epd_display_full();
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

- [ ] **步骤 2：修改添加书签处理**

在 `case 2`（添加书签）中添加反馈：

```c
case 2: // 添加书签
    bookmark_add(s_book_name, s_current_page);
    render_bookmark_added(); // 添加反馈
    page_mgr_pop();
    break;
```

- [ ] **步骤 3：编译验证并 Commit**

```bash
pio run
git add src/ui/page_menu.c
git commit -m "feat(epd): add visual feedback when adding bookmark"
```

---

## 任务 6：实现设置页双模式（page_settings）

**优先级：** P1

**文件：**
- 修改：`src/ui/page_settings.c`

- [ ] **步骤 1：添加编辑模式状态变量**

在文件顶部添加：

```c
static bool s_editing = false;
static int8_t s_edit_value = 0; // 编辑时的临时值
```

- [ ] **步骤 2：修改渲染逻辑**

在渲染函数中添加模式指示：

```c
// 底部提示
if (s_editing) {
    widget_draw_text(20, EPD_HEIGHT - 14, "PREV/NEXT: 调整  HOME: 确认  PWR: 取消");
} else {
    widget_draw_text(20, EPD_HEIGHT - 14, "PREV/NEXT: 移动  HOME: 编辑  PWR: 返回");
}
```

- [ ] **步骤 3：修改按键处理**

重构按键处理逻辑：

```c
static void on_key(KeyId key, KeyEvent event) {
    if (event != KEY_EVT_SHORT) return;

    if (s_editing) {
        // 编辑模式
        switch (key) {
            case KEY_PREV:
                s_edit_value--;
                if (s_edit_value < 0) s_edit_value = get_max_value() - 1;
                on_render();
                break;
            case KEY_NEXT:
                s_edit_value++;
                if (s_edit_value >= get_max_value()) s_edit_value = 0;
                on_render();
                break;
            case KEY_HOME:
                // 确认
                apply_edit_value(s_edit_value);
                s_editing = false;
                on_render();
                break;
            case KEY_PWR:
                // 取消
                s_editing = false;
                on_render();
                break;
        }
    } else {
        // 导航模式
        switch (key) {
            case KEY_PREV:
                if (s_item > 0) s_item--;
                on_render();
                break;
            case KEY_NEXT:
                if (s_item < get_total_items() - 1) s_item++;
                on_render();
                break;
            case KEY_HOME:
                // 进入编辑模式
                s_edit_value = get_current_value();
                s_editing = true;
                on_render();
                break;
            case KEY_PWR:
                page_mgr_pop();
                break;
        }
    }
}
```

- [ ] **步骤 4：添加辅助函数**

实现 `get_max_value()`、`get_current_value()`、`apply_edit_value()` 等辅助函数。

- [ ] **步骤 5：编译验证并 Commit**

```bash
pio run
git add src/ui/page_settings.c
git commit -m "feat(epd): add BROWSE/EDIT dual-mode to settings page"
```

---

## 任务 7：修复跳转页码传递（page_jump + page_reader）

**优先级：** P0

**文件：**
- 修改：`src/ui/page_jump.c`
- 修改：`src/ui/page_reader.h`
- 修改：`src/ui/page_reader.c`

- [ ] **步骤 1：添加 page_reader_set_page 接口**

在 `page_reader.h` 中添加：

```c
void page_reader_set_page(uint32_t target_page);
```

- [ ] **步骤 2：实现 page_reader_set_page**

在 `page_reader.c` 中添加：

```c
void page_reader_set_page(uint32_t target_page) {
    if (target_page < s_page_count) {
        s_current_page = target_page;
    }
}
```

- [ ] **步骤 3：修改 page_jump 跳转逻辑**

修改 `page_jump.c` 中的 KEY_HOME 处理：

```c
case KEY_HOME: {
    uint32_t target = get_target_page();
    if (target < 1) target = 1;
    if (target > s_total_pages) target = s_total_pages;

    // 传递目标页到阅读器
    page_reader_set_page(target - 1);

    // 返回阅读器
    page_mgr_pop();
    page_mgr_pop();
    break;
}
```

- [ ] **步骤 4：编译验证并 Commit**

```bash
pio run
git add src/ui/page_jump.c src/ui/page_reader.c src/ui/page_reader.h
git commit -m "fix(epd): fix page jump not passing target to reader"
```

---

## 任务 8：操作提示居中对齐（page_jump）

**优先级：** P2

**文件：**
- 修改：`src/ui/page_jump.c`

- [ ] **步骤 1：修改操作提示渲染**

找到操作提示渲染代码，改为居中对齐：

```c
// 旧代码：
// widget_draw_text(20, 200, "PREV/NEXT: 调整数字");
// widget_draw_text(20, 220, "HOME: 确认跳转");
// widget_draw_text(20, 240, "PWR: 取消返回");

// 新代码：
const char *hints[] = {
    "PREV/NEXT: 调整数字",
    "HOME: 确认跳转",
    "PWR: 取消返回",
};
int hint_y = 200;
for (int i = 0; i < 3; i++) {
    int hw = widget_text_width(hints[i]);
    widget_draw_text((EPD_WIDTH - hw) / 2, hint_y, hints[i]);
    hint_y += 20;
}
```

- [ ] **步骤 2：编译验证并 Commit**

```bash
pio run
git add src/ui/page_jump.c
git commit -m "style(epd): center-align operation hints in jump page"
```

---

## 任务 9：主页分割线优化（page_home）

**优先级：** P2

**文件：**
- 修改：`src/ui/page_home.c`

- [ ] **步骤 1：添加分割线**

在各区域之间添加分割线：

```c
// 日期区之后
epd_draw_rect(20, DATE_Y + 25, EPD_WIDTH - 40, 1, EPD_COLOR_BLACK);

// 天气区之后
epd_draw_rect(20, WEATHER_Y + 60, EPD_WIDTH - 40, 1, EPD_COLOR_BLACK);

// 统计区之前（已存在）
epd_draw_rect(20, DIVIDER_Y, EPD_WIDTH - 40, 1, EPD_COLOR_BLACK);
```

- [ ] **步骤 2：编译验证并 Commit**

```bash
pio run
git add src/ui/page_home.c
git commit -m "style(epd): add divider lines to home page"
```

---

## 检查清单

- [ ] UTF-8 书名截断已修复（page_bookshelf, page_home）
- [ ] 进度条已添加（page_home, page_reader）
- [ ] 书籍信息弹窗已实现（page_menu）
- [ ] 书签反馈已添加（page_menu）
- [ ] 设置页双模式已实现（page_settings）
- [ ] 跳转页码传递已修复（page_jump, page_reader）
- [ ] 操作提示已居中（page_jump）
- [ ] 分割线已添加（page_home）
- [ ] 所有编译通过
- [ ] 所有功能已测试

---

**计划版本**：v1.0
**创建日期**：2026-06-16
**预计工作量**：9 个任务，约 2-3 小时

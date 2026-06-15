# 妙阅 ESP32 墨水屏电子书阅读器 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 构建基于 ESP32-S3 的 4.2 寸三色墨水屏电子书阅读器，支持 TXT/EPUB 阅读、WiFi 配网、天气显示、网页管理

**架构：** 四层架构——HAL 硬件抽象层（EPD/SD/ADC/RTC/WiFi）→ 核心引擎层（排版/按键/事件总线）→ UI 页面状态机（主页/书架/阅读/设置/休眠）→ 网页服务层（esp_http_server）。PlatformIO 构建，ESP-IDF 框架。

**技术栈：** ESP32-S3 / PlatformIO (ESP-IDF) / FreeType / cJSON / esp_http_server / GxEPD2 或自写 SPI EPD 驱动

**需求文档：** `requires_01.md`

---

## 项目结构

```
ai-bookreader/
├── platformio.ini
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                    # 入口，初始化 + 主循环
│   ├── hal/
│   │   ├── epd.c/h               # E-Ink 驱动（三色 SSD1683）
│   │   ├── sd_card.c/h           # SD 卡 SPI 驱动
│   │   ├── battery.c/h           # ADC 电量检测
│   │   ├── rtc.c/h               # RTC 读写（DS3231 或 SNTP）
│   │   ├── wifi_mgr.c/h          # WiFi STA/AP 管理
│   │   └── keys.c/h              # 按键去抖 + 事件
│   ├── engine/
│   │   ├── types.h               # 公共类型定义（SysConfig/ReaderConfig/BookMeta）
│   │   ├── config.c/h            # NVS 配置持久化
│   │   ├── event_bus.c/h         # 观察者模式事件总线
│   │   ├── typesetter.c/h        # FreeType 排版引擎 + 分页算法
│   │   └── book_parser.c/h       # TXT/EPUB 解析
│   ├── ui/
│   │   ├── page_mgr.c/h          # 页面状态机（页面栈 + 切换）
│   │   ├── widget.c/h            # 基础控件（文本/图标/进度条）
│   │   ├── status_bar.c/h        # 全局状态栏（20px）
│   │   ├── page_home.c/h         # 主页
│   │   ├── page_bookshelf.c/h    # 书架页
│   │   ├── page_reader.c/h       # 阅读界面
│   │   ├── page_settings.c/h     # 设置页
│   │   ├── page_sleep.c/h        # 休眠界面（3 种布局）
│   │   └── page_boot.c/h         # 首次配网引导页
│   ├── service/
│   │   ├── weather.c/h           # 和风天气 API
│   │   ├── web_server.c/h        # esp_http_server 路由
│   │   └── ota.c/h               # OTA 升级
│   └── resources/
│       └── ui_font_14.bin        # 点阵字体
├── web/                          # 网页前端（构建后 gzip 存入 SPIFFS）
│   ├── index.html
│   ├── app.js
│   └── style.css
├── data/                         # SPIFFS 分区内容
│   ├── poems.json
│   └── fonts/
│       └── ui_font_14.bin
└── docs/
    └── superpowers/plans/
        └── 2025-06-15-miaoyue-reader.md
```

---

## Phase 1：基础框架（~3 周）

### 任务 1.1：PlatformIO 项目初始化

**文件：**
- 创建：`platformio.ini`
- 创建：`CMakeLists.txt`
- 创建：`main/CMakeLists.txt`
- 创建：`main/main.c`

- [ ] **步骤 1：创建 PlatformIO 项目配置**

```ini
; platformio.ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = esp-idf
board_build.flash_size = 16MB
board_build.partitions = default_16MB.csv
monitor_speed = 115200
```

- [ ] **步骤 2：创建顶层 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ai-bookreader)
```

- [ ] **步骤 3：创建 main 组件 CMakeLists.txt**

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_netif nvs_flash
)
```

- [ ] **步骤 4：编写 main.c 入口骨架**

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "妙阅 电子书阅读器启动");
    
    // NVS 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // TODO: HAL 初始化
    // TODO: 加载配置
    // TODO: 启动 UI 状态机
    // TODO: 启动网页服务

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

- [ ] **步骤 5：编译验证**

运行：`pio run`
预期：编译成功，无错误

- [ ] **步骤 6：Commit**

```bash
git add platformio.ini CMakeLists.txt main/
git commit -m "feat: 初始化 PlatformIO ESP-IDF 项目骨架"
```

---

### 任务 1.2：公共类型定义

**文件：**
- 创建：`main/engine/types.h`

- [ ] **步骤 1：定义核心数据结构**

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

// 系统配置（持久化到 NVS）
typedef struct {
    char     ssid[64];
    char     password[64];
    char     weatherApiKey[64];
    char     weatherCity[32];
    uint8_t  sleepLayout;       // 0=时钟天气 1=风景图 2=诗词
    uint8_t  displayRotation;   // 0/1/2/3 → 0°/90°/180°/270°
    uint32_t sleepTimeoutSec;   // 无操作自动休眠秒数
    bool     initialized;       // 首次配网标志
} SysConfig;

// 阅读器设置（持久化到 NVS）
typedef struct {
    uint8_t  fontSize;          // 16/20/24/28/32px
    uint8_t  fontId;            // 字体索引
    uint8_t  lineSpacing;       // 1.0/1.2/1.5/2.0 (×10存储)
    uint8_t  charSpacing;       // 0–8px
    uint8_t  margin;            // 页边距 px
} ReaderConfig;

// 书目元数据（存储于 SD:/books/metadata.json）
typedef struct {
    char     id[16];
    char     title[128];
    char     author[64];
    char     filePath[256];
    uint32_t totalPages;
    uint32_t currentPage;
    uint32_t readingSeconds;
    uint8_t  coverOffset;
} BookMeta;

// 页面 ID
typedef enum {
    PAGE_BOOT,
    PAGE_HOME,
    PAGE_BOOKSHELF,
    PAGE_READER,
    PAGE_SETTINGS,
    PAGE_SLEEP,
} PageId;

// 按键事件
typedef enum {
    KEY_EVT_SHORT,      // 短按 (<500ms)
    KEY_EVT_LONG,       // 长按 (>800ms, 500ms 重复)
    KEY_EVT_SUPER_LONG, // 超长按 (>2000ms, 仅 PWR)
    KEY_EVT_COMBO,      // 组合按
} KeyEvent;

typedef enum {
    KEY_PWR,
    KEY_PREV,
    KEY_NEXT,
    KEY_HOME,
    KEY_COUNT,
} KeyId;

// 事件总线事件类型
typedef enum {
    EVT_KEY,            // 按键事件
    EVT_PAGE_SWITCH,    // 页面切换
    EVT_CONFIG_CHANGED, // 配置变更
    EVT_WIFI_STATUS,    // WiFi 状态变化
    EVT_BATTERY,        // 电量变化
    EVT_SLEEP,          // 进入休眠
    EVT_WAKE,           // 唤醒
} EventType;
```

- [ ] **步骤 2：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 3：Commit**

```bash
git add main/engine/types.h
git commit -m "feat: 定义核心数据结构和类型枚举"
```

---

### 任务 1.3：NVS 配置持久化

**文件：**
- 创建：`main/engine/config.c`
- 创建：`main/engine/config.h`
- 修改：`main/CMakeLists.txt`（添加 SRCS）
- 修改：`main/main.c`（调用初始化）

- [ ] **步骤 1：编写 config.h 接口**

```c
#pragma once
#include "types.h"

esp_err_t config_init(void);
esp_err_t config_load_sys(SysConfig *cfg);
esp_err_t config_save_sys(const SysConfig *cfg);
esp_err_t config_load_reader(ReaderConfig *cfg);
esp_err_t config_save_reader(const ReaderConfig *cfg);
esp_err_t config_factory_reset(void);
```

- [ ] **步骤 2：编写 config.c 实现**

使用 NVS blob 读写 `SysConfig` 和 `ReaderConfig`，首次启动时写入默认值（`initialized = false`）。

- [ ] **步骤 3：在 main.c 中调用 config_init()**

- [ ] **步骤 4：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 5：Commit**

```bash
git add main/engine/config.c main/engine/config.h main/CMakeLists.txt main/main.c
git commit -m "feat: NVS 配置持久化（SysConfig/ReaderConfig）"
```

---

### 任务 1.4：E-Ink 驱动（三色 SSD1683）

**文件：**
- 创建：`main/hal/epd.c`
- 创建：`main/hal/epd.h`
- 修改：`main/CMakeLists.txt`
- 修改：`main/main.c`

- [ ] **步骤 1：编写 epd.h 接口**

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

// 帧缓冲区：400×300 像素，每像素 2bit (00=黑 01=白 10=红)
#define EPD_WIDTH  400
#define EPD_HEIGHT 300
#define EPD_BUF_SIZE (EPD_WIDTH * EPD_HEIGHT / 4)  // 2bit per pixel

typedef enum {
    EPD_COLOR_BLACK = 0,
    EPD_COLOR_WHITE = 1,
    EPD_COLOR_RED   = 2,
} EpdColor;

esp_err_t epd_init(void);
void epd_clear(EpdColor color);
void epd_set_pixel(int x, int y, EpdColor color);
void epd_draw_rect(int x, int y, int w, int h, EpdColor color);
void epd_fill_rect(int x, int y, int w, int h, EpdColor color);
void epd_display_full(void);     // 三色全刷 ~15s
void epd_display_partial(void);  // 黑白局部刷新 ~250ms
void epd_sleep(void);            // 进入低功耗模式
void epd_set_rotation(uint8_t rot);  // 0/1/2/3
```

- [ ] **步骤 2：实现 SSD1683 SPI 驱动**

初始化 SPI 总线，实现 LUT 加载、帧缓冲发送、全刷/局部刷新命令序列。黑白局部刷新仅发送 BW plane，三色全刷同时发送 BW + RW plane。

- [ ] **步骤 3：在 main.c 中测试全刷/局部刷新**

上电后全屏刷白 → 全屏刷黑 → 局部画红色矩形 → 全刷显示。

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload`
预期：屏幕依次显示白色 → 黑色 → 红色矩形

- [ ] **步骤 5：Commit**

```bash
git add main/hal/epd.c main/hal/epd.h
git commit -m "feat: SSD1683 三色 E-Ink 驱动（全刷/局部刷新）"
```

---

### 任务 1.5：按键驱动

**文件：**
- 创建：`main/hal/keys.c`
- 创建：`main/hal/keys.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 keys.h 接口**

```c
#pragma once
#include "engine/types.h"

typedef void (*key_callback_t)(KeyId key, KeyEvent event);

esp_err_t keys_init(key_callback_t cb);
```

- [ ] **步骤 2：实现按键状态机**

GPIO 中断 + 定时器轮询实现去抖（10ms），检测短按（<500ms）、长按（>800ms，500ms 重复）、超长按（>2000ms）。组合按键误触窗口 100ms。

- [ ] **步骤 3：在 main.c 中注册按键回调，打印按键事件**

- [ ] **步骤 4：编译烧录，按键测试**

运行：`pio run -t upload && pio device monitor`
预期：按下各键打印对应事件类型

- [ ] **步骤 5：Commit**

```bash
git add main/hal/keys.c main/hal/keys.h
git commit -m "feat: 按键驱动（短按/长按/超长按/组合）"
```

---

### 任务 1.6：ADC 电量检测

**文件：**
- 创建：`main/hal/battery.c`
- 创建：`main/hal/battery.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 battery.h 接口**

```c
#pragma once
#include <stdint.h>

esp_err_t battery_init(void);
uint8_t battery_get_percent(void);   // 0-100
bool battery_is_charging(void);
bool battery_is_low(void);           // < 5%
bool battery_is_warning(void);       // < 15%
```

- [ ] **步骤 2：实现 ADC 采样 + 电压-SOC 映射**

16 次采样取平均，分压比校准，锂电池放电曲线查表。

- [ ] **步骤 3：在 main.c 中周期读取电量（5s 间隔），打印到串口**

- [ ] **步骤 4：编译验证**

运行：`pio run`
预期：串口周期输出电量百分比

- [ ] **步骤 5：Commit**

```bash
git add main/hal/battery.c main/hal/battery.h
git commit -m "feat: ADC 电量检测（16次采样 + SOC映射）"
```

---

### 任务 1.7：RTC 时间

**文件：**
- 创建：`main/hal/rtc.c`
- 创建：`main/hal/rtc.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 rtc.h 接口**

```c
#pragma once
#include <stdint.h>
#include <time.h>

esp_err_t rtc_init(void);
esp_err_t rtc_get_time(struct tm *timeinfo);
esp_err_t rtc_set_time(const struct tm *timeinfo);
```

- [ ] **步骤 2：实现 I2C DS3231 驱动**

BCD 编码读写，支持秒/分/时/日/月/年。

- [ ] **步骤 3：在 main.c 中读取并打印时间**

- [ ] **步骤 4：编译验证**

运行：`pio run`
预期：串口输出当前时间

- [ ] **步骤 5：Commit**

```bash
git add main/hal/rtc.c main/hal/rtc.h
git commit -m "feat: DS3231 RTC 驱动"
```

---

### 任务 1.8：SD 卡驱动

**文件：**
- 创建：`main/hal/sd_card.c`
- 创建：`main/hal/sd_card.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 sd_card.h 接口**

```c
#pragma once
#include <stdbool.h>

esp_err_t sd_card_init(void);
bool sd_card_is_mounted(void);
const char* sd_card_get_mount_point(void);  // 返回 "/sdcard"
void sd_card_deinit(void);
```

- [ ] **步骤 2：实现 SPI SD 卡挂载**

使用 ESP-IDF `sdmmc` + `fatfs` 组件，SPI 模式挂载。

- [ ] **步骤 3：在 main.c 中挂载 SD 卡，列出根目录文件**

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload && pio device monitor`
预期：串口输出 SD 卡文件列表

- [ ] **步骤 5：Commit**

```bash
git add main/hal/sd_card.c main/hal/sd_card.h
git commit -m "feat: SPI SD 卡驱动（FATFS 挂载）"
```

---

### 任务 1.9：事件总线

**文件：**
- 创建：`main/engine/event_bus.c`
- 创建：`main/engine/event_bus.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 event_bus.h 接口**

```c
#pragma once
#include "types.h"

typedef void (*event_handler_t)(EventType type, void *data);

esp_err_t event_bus_init(void);
esp_err_t event_bus_subscribe(EventType type, event_handler_t handler);
esp_err_t event_bus_unsubscribe(EventType type, event_handler_t handler);
esp_err_t event_bus_publish(EventType type, void *data);
```

- [ ] **步骤 2：实现观察者模式**

静态数组存储订阅者（每事件类型最多 8 个），publish 时同步调用所有 handler。

- [ ] **步骤 3：在 main.c 中测试：按键回调 → 发布 EVT_KEY → handler 打印**

- [ ] **步骤 4：编译验证**

运行：`pio run`
预期：按键触发事件总线回调

- [ ] **步骤 5：Commit**

```bash
git add main/engine/event_bus.c main/engine/event_bus.h
git commit -m "feat: 观察者模式事件总线"
```

---

### 任务 1.10：页面状态机骨架

**文件：**
- 创建：`main/ui/page_mgr.c`
- 创建：`main/ui/page_mgr.h`
- 修改：`main/CMakeLists.txt`
- 修改：`main/main.c`

- [ ] **步骤 1：编写 page_mgr.h 接口**

```c
#pragma once
#include "engine/types.h"

// 页面虚函数表
typedef struct {
    PageId id;
    void (*on_enter)(void);
    void (*on_exit)(void);
    void (*on_key)(KeyId key, KeyEvent event);
    void (*on_render)(void);
} PageVtbl;

esp_err_t page_mgr_init(void);
esp_err_t page_mgr_register(const PageVtbl *page);
esp_err_t page_mgr_switch(PageId id);
PageId page_mgr_current(void);
```

- [ ] **步骤 2：实现页面栈管理**

支持 push/pop（用于菜单弹出）和 switch（页面切换）。切换时调用 on_exit → on_enter，触发 EPD 全刷。

- [ ] **步骤 3：在 main.c 中注册空白页面，实现 BOOT → HOME 切换**

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload && pio device monitor`
预期：串口输出页面切换日志

- [ ] **步骤 5：Commit**

```bash
git add main/ui/page_mgr.c main/ui/page_mgr.h
git commit -m "feat: 页面状态机骨架（push/pop/switch）"
```

---

## Phase 2：核心 UI（~4 周）

### 任务 2.1：点阵字体渲染

**文件：**
- 创建：`main/ui/widget.c`
- 创建：`main/ui/widget.h`
- 创建：`main/resources/ui_font_14.bin`（预生成）
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 widget.h 基础接口**

```c
#pragma once
#include "hal/epd.h"

void widget_draw_char(int x, int y, char c, EpdColor color);
void widget_draw_text(int x, int y, const char *text, EpdColor color);
int widget_text_width(const char *text);
void widget_draw_icon_16(int x, int y, const uint16_t *icon, EpdColor color);
```

- [ ] **步骤 2：实现 14px 点阵字体查找 + 渲染**

从 SPIFFS 加载 `ui_font_14.bin` 到 PSRAM，按 Unicode 码点查找位图，逐像素写入 EPD 帧缓冲。

- [ ] **步骤 3：在 main 中测试：显示 "妙阅 Hello 123"**

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload`
预期：屏幕显示中文 + ASCII 文本

- [ ] **步骤 5：Commit**

```bash
git add main/ui/widget.c main/ui/widget.h
git commit -m "feat: 14px 点阵字体渲染（SPIFFS 加载）"
```

---

### 任务 2.2：全局状态栏

**文件：**
- 创建：`main/ui/status_bar.c`
- 创建：`main/ui/status_bar.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 status_bar.h 接口**

```c
#pragma once

void status_bar_init(void);
void status_bar_render(void);       // 绘制到 EPD 帧缓冲
void status_bar_update_time(void);  // 仅更新时间区域
void status_bar_update_battery(void); // 仅更新电量区域
```

- [ ] **步骤 2：实现状态栏绘制**

高度 20px，全宽黑底白字。左侧 HH:MM（RTC），右侧电量% + WiFi 图标。电量 <20% 用红色。

- [ ] **步骤 3：在 main 中测试状态栏渲染**

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload`
预期：屏幕顶部显示状态栏（时间 + 电量 + WiFi 状态）

- [ ] **步骤 5：Commit**

```bash
git add main/ui/status_bar.c main/ui/status_bar.h
git commit -m "feat: 全局状态栏（时间/电量/WiFi）"
```

---

### 任务 2.3：FreeType 排版引擎

**文件：**
- 创建：`main/engine/typesetter.c`
- 创建：`main/engine/typesetter.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 typesetter.h 接口**

```c
#pragma once
#include "engine/types.h"
#include "hal/epd.h"

typedef struct {
    uint32_t startByte;     // 页起始字节偏移
    uint32_t startLine;     // 页起始行号
    uint16_t pageNum;
} PageIndex;

typedef struct {
    const uint8_t *ttf_data;    // TTF 字体数据指针
    uint32_t ttf_size;
    uint8_t  fontSize;
    uint8_t  lineSpacing;       // ×10
    uint8_t  charSpacing;
    uint8_t  margin;
    int      pageWidth;         // 可用宽度
    int      pageHeight;        // 可用高度
} TypesetterConfig;

esp_err_t typesetter_init(const TypesetterConfig *cfg);
esp_err_t typesetter_paginate(const char *text, uint32_t textLen, PageIndex **pages, uint32_t *pageCount);
esp_err_t typesetter_render_page(const char *text, const PageIndex *page, int yOffset);
```

- [ ] **步骤 2：实现 FreeType 初始化 + 字形测量**

加载 TTF 到 PSRAM，FreeType 初始化，按字号渲染 glyph，测量宽度。

- [ ] **步骤 3：实现分页算法**

按段落切分 → 逐字符测量断行 → 累积行高分页 → 生成 PageIndex 数组。段落不跨页，孤行控制。

- [ ] **步骤 4：实现页面渲染**

给定 PageIndex，从 text 偏移处开始，逐行渲染到 EPD 帧缓冲指定区域。

- [ ] **步骤 5：在 main 中测试：加载 TXT 文件，渲染第一页**

- [ ] **步骤 6：编译烧录验证**

运行：`pio run -t upload`
预期：屏幕显示排版后的中文正文

- [ ] **步骤 7：Commit**

```bash
git add main/engine/typesetter.c main/engine/typesetter.h
git commit -m "feat: FreeType 排版引擎 + 分页算法"
```

---

### 任务 2.4：TXT/EPUB 解析器

**文件：**
- 创建：`main/engine/book_parser.c`
- 创建：`main/engine/book_parser.h`
- 修改：`main/CMakeLists.txt`

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
esp_err_t book_load_text(const char *filePath, char **text, uint32_t *textLen, BookFormat *format);
esp_err_t book_extract_metadata(const char *filePath, BookMeta *meta);
void book_free_text(char *text);
```

- [ ] **步骤 2：实现 TXT 解析**

自动检测 UTF-8 / GBK 编码（BOM 检测 + 启发式），GBK 转 UTF-8。

- [ ] **步骤 3：实现 EPUB 纯文本提取**

解压 ZIP → 解析 `content.opf` 找到 XHTML 文件 → 去 HTML 标签 → 拼接纯文本。

- [ ] **步骤 4：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 5：Commit**

```bash
git add main/engine/book_parser.c main/engine/book_parser.h
git commit -m "feat: TXT/EPUB 解析器（UTF-8/GBK 自动检测）"
```

---

### 任务 2.5：主页

**文件：**
- 创建：`main/ui/page_home.c`
- 创建：`main/ui/page_home.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：实现主页布局**

状态栏下方：日期区（居中 18px）→ 天气区（占位，后续接入 API）→ 统计区（藏书数/今日阅读/正在读/累计阅读，数据从 metadata.json 读取）。

- [ ] **步骤 2：注册到页面状态机，实现 HOME 键进入书架**

- [ ] **步骤 3：编译烧录验证**

运行：`pio run -t upload`
预期：主页显示日期 + 统计数据

- [ ] **步骤 4：Commit**

```bash
git add main/ui/page_home.c main/ui/page_home.h
git commit -m "feat: 主页（日期 + 统计）"
```

---

### 任务 2.6：书架页

**文件：**
- 创建：`main/ui/page_bookshelf.c`
- 创建：`main/ui/page_bookshelf.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：实现书架网格布局**

3×2 网格，每格 100×130px。扫描 SD:/books/ 目录，读取 metadata.json，渲染封面（JPG 解码或占位图）+ 书名（2 行截断）。

- [ ] **步骤 2：实现翻页 + 光标导航**

PREV/NEXT 移动光标，长按翻页。选中项红色边框（全刷时更新）。

- [ ] **步骤 3：HOME 键打开选中书籍 → 切换到阅读页**

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload`
预期：书架显示书籍网格，可导航选择

- [ ] **步骤 5：Commit**

```bash
git add main/ui/page_bookshelf.c main/ui/page_bookshelf.h
git commit -m "feat: 书架页（3×2 网格 + 封面 + 翻页）"
```

---

### 任务 2.7：阅读界面

**文件：**
- 创建：`main/ui/page_reader.c`
- 创建：`main/ui/page_reader.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：实现阅读界面布局**

状态栏（显示章节标题）+ 正文区（262px）+ 底部进度条（页码 + 百分比 + 进度条）。

- [ ] **步骤 2：实现翻页逻辑**

PREV/NEXT 短按翻 1 页（局部刷新），长按翻 5 页。每 5 次局部刷新触发一次黑白全刷清洁。阅读位置按页持久化到 NVS。

- [ ] **步骤 3：实现 HOME 键上下文菜单**

弹出菜单：继续阅读 / 跳转到... / 添加书签 / 书籍信息。菜单使用 push 进页面栈。

- [ ] **步骤 4：实现字体/字号热切换**

设置变更时重新排版，保持当前位置（按字节偏移定位）。

- [ ] **步骤 5：编译烧录验证**

运行：`pio run -t upload`
预期：可阅读 TXT 文件，翻页流畅

- [ ] **步骤 6：Commit**

```bash
git add main/ui/page_reader.c main/ui/page_reader.h
git commit -m "feat: 阅读界面（翻页/进度条/上下文菜单）"
```

---

### 任务 2.8：设置页

**文件：**
- 创建：`main/ui/page_settings.c`
- 创建：`main/ui/page_settings.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：实现设置菜单 UI**

两个子菜单：阅读排版（字体/字号/行距/字间距/页边距）+ 系统（显示方向/自动休眠/清洁屏幕/恢复出厂/关于）。

- [ ] **步骤 2：实现导航逻辑**

PREV/NEXT 移动，HOME 确认，长按修改数值。变更立即保存到 NVS 并生效。

- [ ] **步骤 3：编译烧录验证**

运行：`pio run -t upload`
预期：可浏览和修改设置项

- [ ] **步骤 4：Commit**

```bash
git add main/ui/page_settings.c main/ui/page_settings.h
git commit -m "feat: 设置页（阅读排版 + 系统设置）"
```

---

## Phase 3：网络与服务（~3 周）

### 任务 3.1：WiFi 管理

**文件：**
- 创建：`main/hal/wifi_mgr.c`
- 创建：`main/hal/wifi_mgr.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 wifi_mgr.h 接口**

```c
#pragma once
#include <stdbool.h>

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,
} WifiState;

typedef void (*wifi_state_cb_t)(WifiState state, const char *ip);

esp_err_t wifi_mgr_init(wifi_state_cb_t cb);
esp_err_t wifi_mgr_connect(const char *ssid, const char *password);
esp_err_t wifi_mgr_start_ap(const char *ssid, const char *password);
esp_err_t wifi_mgr_disconnect(void);
WifiState wifi_mgr_get_state(void);
const char* wifi_mgr_get_ip(void);
esp_err_t wifi_mgr_scan(void (*result_cb)(const char *ssid, int rssi));
```

- [ ] **步骤 2：实现 STA 连接 + AP 模式**

STA 超时 10s，失败自动切 AP。AP SSID `EReader-XXXX`，密码 `ereader123`，IP `192.168.4.1`。

- [ ] **步骤 3：编译烧录验证**

运行：`pio run -t upload && pio device monitor`
预期：可连接已保存 WiFi 或进入 AP 模式

- [ ] **步骤 4：Commit**

```bash
git add main/hal/wifi_mgr.c main/hal/wifi_mgr.h
git commit -m "feat: WiFi STA/AP 管理"
```

---

### 任务 3.2：SNTP 时间同步

**文件：**
- 修改：`main/hal/rtc.c`（添加 SNTP 同步）
- 修改：`main/main.c`

- [ ] **步骤 1：WiFi 连接成功后启动 SNTP**

使用 `esp_sntp`，同步成功后写入 DS3231 RTC。

- [ ] **步骤 2：编译验证**

运行：`pio run`
预期：WiFi 连接后时间自动同步

- [ ] **步骤 3：Commit**

```bash
git add main/hal/rtc.c main/main.c
git commit -m "feat: SNTP 时间同步 + 写入 RTC"
```

---

### 任务 3.3：和风天气 API

**文件：**
- 创建：`main/service/weather.c`
- 创建：`main/service/weather.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：编写 weather.h 接口**

```c
#pragma once

typedef struct {
    char city[32];
    char text[32];          // 天气描述
    int  temp;              // 当前温度
    int  tempMin;
    int  tempMax;
    int  humidity;
    char airLevel[16];      // 空气质量
    char alert[64];         // 预警信息
    uint32_t updateAge;     // 数据龄（分钟）
} WeatherData;

esp_err_t weather_init(const char *apiKey, const char *city);
esp_err_t weather_refresh(void);
const WeatherData* weather_get(void);
```

- [ ] **步骤 2：实现 HTTP 请求 + JSON 解析**

使用 `esp_http_client` 请求实时天气 + 空气质量，cJSON 解析响应，缓存到 NVS。

- [ ] **步骤 3：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 4：Commit**

```bash
git add main/service/weather.c main/service/weather.h
git commit -m "feat: 和风天气 API 集成（实时天气 + 空气质量）"
```

---

### 任务 3.4：主页接入天气数据

**文件：**
- 修改：`main/ui/page_home.c`

- [ ] **步骤 1：将天气占位符替换为真实天气数据**

调用 `weather_get()` 获取数据，渲染天气图标 + 温度 + 天气描述 + 空气质量。离线时显示缓存 + 时间戳。

- [ ] **步骤 2：编译烧录验证**

运行：`pio run -t upload`
预期：主页显示实时天气

- [ ] **步骤 3：Commit**

```bash
git add main/ui/page_home.c
git commit -m "feat: 主页接入和风天气数据"
```

---

### 任务 3.5：网页服务器

**文件：**
- 创建：`main/service/web_server.c`
- 创建：`main/service/web_server.h`
- 修改：`main/CMakeLists.txt`
- 修改：`main/main.c`

- [ ] **步骤 1：编写 web_server.h 接口**

```c
#pragma once

esp_err_t web_server_init(void);
esp_err_t web_server_stop(void);
```

- [ ] **步骤 2：实现 esp_http_server 路由**

- `GET /` → 返回 SPIFFS 中的 `index.html.gz`
- `GET /api/books` → 返回书籍列表 JSON
- `POST /api/books/upload` → multipart 文件上传到 SD
- `DELETE /api/books/:id` → 删除书籍
- `GET /api/config` → 返回系统配置
- `POST /api/config/wifi` → 更新 WiFi 配置
- `POST /api/config/weather` → 更新天气配置
- `POST /api/config/sleep` → 更新休眠界面配置
- `GET /api/weather` → 返回天气数据
- `POST /update` → OTA 固件上传

- [ ] **步骤 3：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 4：Commit**

```bash
git add main/service/web_server.c main/service/web_server.h
git commit -m "feat: esp_http_server 路由（书架/WiFi/天气/OTA）"
```

---

### 任务 3.6：网页前端

**文件：**
- 创建：`web/index.html`
- 创建：`web/app.js`
- 创建：`web/style.css`
- 修改：`platformio.ini`（添加 SPIFFS 构建脚本）

- [ ] **步骤 1：实现书架管理页面**

书籍列表（标题/作者/大小/最后阅读时间）、上传按钮（TXT/EPUB，最大 50MB）、删除、拖拽排序。

- [ ] **步骤 2：实现 WiFi 配置页面**

扫描附近 WiFi → 选择 → 输入密码 → 连接。显示当前状态/IP/信号强度。

- [ ] **步骤 3：实现天气配置页面**

API Key 输入、城市搜索、更新间隔选择。

- [ ] **步骤 4：实现休眠界面配置**

三种布局单选：时钟天气 / 风景图片上传 / 诗词选择。

- [ ] **步骤 5：构建 gzip 压缩并烧录 SPIFFS**

运行：`pio run -t uploadfs`

- [ ] **步骤 6：Commit**

```bash
git add web/ platformio.ini
git commit -m "feat: 网页配置前端（书架/WiFi/天气/休眠）"
```

---

### 任务 3.7：首次配网引导页

**文件：**
- 创建：`main/ui/page_boot.c`
- 创建：`main/ui/page_boot.h`
- 修改：`main/main.c`

- [ ] **步骤 1：实现配网引导页**

检测 `initialized == false` → 显示 AP 配网信息（SSID + 密码）→ 启动 AP + Web Server → 等待配置完成 → 设置 `initialized = true` → 重启。

- [ ] **步骤 2：编译烧录验证**

运行：`pio run -t upload`
预期：首次开机显示配网页面

- [ ] **步骤 3：Commit**

```bash
git add main/ui/page_boot.c main/ui/page_boot.h main/main.c
git commit -m "feat: 首次配网引导页（AP 模式）"
```

---

## Phase 4：完善与优化（~2 周）

### 任务 4.1：休眠界面

**文件：**
- 创建：`main/ui/page_sleep.c`
- 创建：`main/ui/page_sleep.h`
- 修改：`main/CMakeLists.txt`

- [ ] **步骤 1：实现布局 A（时钟 + 天气）**

大字时钟 + 日期 + 天气摘要，居中布局。

- [ ] **步骤 2：实现布局 B（风景图片）**

从 SD:/sleep/custom.bmp 加载三色 BMP，或显示内置默认图。可选叠加时间。

- [ ] **步骤 3：实现布局 C（诗词 + 时间）**

从 SPIFFS poems.json 随机选取，居中排版 + 底部时间。

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload`
预期：三种休眠布局正常显示

- [ ] **步骤 5：Commit**

```bash
git add main/ui/page_sleep.c main/ui/page_sleep.h
git commit -m "feat: 休眠界面三种布局（时钟/风景/诗词）"
```

---

### 任务 4.2：电源管理

**文件：**
- 修改：`main/main.c`
- 修改：`main/hal/wifi_mgr.c`

- [ ] **步骤 1：实现自动休眠定时器**

无操作计时器，超时后：刷新休眠界面 → 关闭 WiFi/SD/外设 → 保存阅读位置 → Deep Sleep。

- [ ] **步骤 2：实现 PWR 键唤醒**

BTN_PWR 配置为 Deep Sleep 唤醒源，唤醒后正常启动，恢复到休眠前页面。

- [ ] **步骤 3：实现低电量保护**

<5% 强制保存 + 显示"电量不足" + Deep Sleep。

- [ ] **步骤 4：编译烧录验证**

运行：`pio run -t upload`
预期：自动休眠和唤醒正常工作

- [ ] **步骤 5：Commit**

```bash
git add main/main.c main/hal/wifi_mgr.c
git commit -m "feat: 电源管理（自动休眠/唤醒/低电保护）"
```

---

### 任务 4.3：OTA 升级

**文件：**
- 创建：`main/service/ota.c`
- 创建：`main/service/ota.h`
- 修改：`main/service/web_server.c`

- [ ] **步骤 1：实现 OTA 固件上传**

`POST /update` 接收 multipart 固件，`esp_ota` 写入 OTA 分区，校验后切换启动分区。

- [ ] **步骤 2：实现进度反馈 + 错误处理**

上传进度返回百分比，失败回滚。

- [ ] **步骤 3：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 4：Commit**

```bash
git add main/service/ota.c main/service/ota.h main/service/web_server.c
git commit -m "feat: OTA 固件升级（网页上传）"
```

---

### 任务 4.4：看门狗 + 崩溃恢复

**文件：**
- 修改：`main/main.c`

- [ ] **步骤 1：启用 Task WDT**

主循环和关键任务注册到看门狗，超时自动重启。

- [ ] **步骤 2：实现重启恢复**

重启后检测 NVS 中的阅读位置，自动恢复到上次阅读页面。

- [ ] **步骤 3：编译验证**

运行：`pio run`
预期：编译成功

- [ ] **步骤 4：Commit**

```bash
git add main/main.c
git commit -m "feat: 看门狗 + 崩溃恢复"
```

---

### 任务 4.5：全场景测试

- [ ] **步骤 1：按键全场景测试**

覆盖所有页面 × 所有按键 × 所有事件类型的组合。

- [ ] **步骤 2：续航测试**

纯阅读模式（WiFi 关闭）测量电流和续航时间。

- [ ] **步骤 3：边界测试**

- SD 卡无书籍时书架显示
- WiFi 连接失败时离线模式
- 电量耗尽时的保护行为
- 大文件（>10MB TXT）翻页性能
- metadata.json 损坏恢复

- [ ] **步骤 4：Commit**

```bash
git commit -m "test: 全场景集成测试"
```

---

## 自检清单

- [ ] 所有硬件驱动（EPD/SD/ADC/RTC/WiFi/Keys）均有独立任务
- [ ] 排版引擎支持 TXT/EPUB + 自动分页 + 孤行控制
- [ ] 三色屏幕策略：局部刷新仅黑白，红色合并到全刷
- [ ] 网页前端覆盖书架管理/WiFi/天气/休眠配置
- [ ] 电源管理覆盖活跃/阅读/轻睡/深睡四种模式
- [ ] OTA + 看门狗 + 崩溃恢复
- [ ] 需求文档 §1-§12 所有功能点均有对应任务

# 妙阅（ai-bookreader）架构设计文档

> 基于 ESP32-S3 + 4.2 寸三色墨水屏的电子书阅读器
> 当前进度：Phase 1（HAL/事件总线/页面状态机）+ Phase 2（核心 UI/排版/解析/各页面）已完成

---

## 1. 项目概述

### 1.1 目标
构建低功耗电子书阅读器，支持 TXT/EPUB 阅读、WiFi 配网、天气显示、网页管理。
- **硬件**：ESP32-S3-DevKitC-1，4.2 寸 SSD1683 三色墨水屏（400×300，黑白红）
- **续航**：1500mAh 电池，待机功耗 < 1mA
- **存储**：SD 卡存放书籍，NVS 存配置，SPIFFS 暂未启用
- **交互**：5 键（PWR/PREV/NEXT/HOME/自定义），全局状态栏 + 多页面状态机

### 1.2 当前状态（2026-06-16）
- **已完成**：Phase 1-4（HAL/引擎/核心 UI/排版/解析/休眠/WiFi/天气/OTA/网页服务）
- **进行中**：Phase 5（优化与打磨）
- **构建状态**：编译通过，固件 21.0% Flash（660KB / 3MB）、14.8% RAM（48KB / 320KB）

---

## 2. 硬件规格

### 2.1 主控
| 项目 | 规格 |
|------|------|
| SoC | ESP32-S3（N16R8：16MB Flash，8MB PSRAM） |
| CPU | Xtensa LX7 双核 240MHz |
| 内存 | 512KB SRAM + 8MB PSRAM |
| 存储 | 16MB QD Flash（实际 3MB app 分区） |
| 无线 | 2.4GHz WiFi（Phase 3+） |

> **注意**：硬件有 8MB PSRAM，大缓冲（字模、TTF、书籍文本）可通过 `heap_caps_malloc(MALLOC_CAP_SPIRAM)` 申请到 PSRAM，不影响内部 SRAM。

### 2.2 显示器
- **型号**：4.2 寸三色墨水屏（SSD1683 控制器）
- **分辨率**：400×300，黑白红三色
- **接口**：SPI
- **GPIO**：
  - CS=GPIO5, CLK=GPIO6, MOSI=GPIO7, MISO=GPIO2
  - DC=GPIO3, RST=GPIO4, BUSY=GPIO1
- **刷新策略**：
  - 全刷（黑/白/红三色，~15s）
  - 局部刷新（黑白，~250ms）
  - 5 次局部刷新后触发 1 次全刷（避免残影）

### 2.3 其他外设
| 模块 | 接口 | 状态 |
|------|------|------|
| DS3231 RTC | I²C | ✓ 已实现（任务 1.7） |
| ADC 电量检测 | ADC1 | ✓ 已实现（任务 1.6，16 次采样 + SOC 映射） |
| SD 卡 | SPI | ✓ 已实现（任务 1.8，FATFS） |
| 按键 | GPIO + 中断 | ✓ 已实现（任务 1.5，短按/长按/超长按/组合） |
| WiFi STA/AP | - | ✗ Phase 3+ 待实现 |

---

## 3. 软件架构（四层）

### 3.1 分层概览

```
┌─────────────────────────────────────────────────┐
│  Application Layer (main.c)                     │
│  - 初始化序列 / 任务调度                        │
├─────────────────────────────────────────────────┤
│  UI Layer (src/ui/)                             │
│  - page_mgr: 页面状态机（注册/切换/按键分发）  │
│  - widget: 点阵字体渲染                         │
│  - status_bar: 全局状态栏（时间/电量/WiFi）    │
│  - page_*: 4 个页面（home/bookshelf/reader/    │
│    settings）                                   │
├─────────────────────────────────────────────────┤
│  Engine Layer (src/engine/)                     │
│  - config: NVS 配置持久化（Sys/Reader）         │
│  - event_bus: 观察者模式事件总线               │
│  - typesetter: FreeType 排版 + 预分页          │
│  - book_parser: TXT/EPUB 解析                   │
├─────────────────────────────────────────────────┤
│  HAL Layer (src/hal/)                           │
│  - epd: E-Ink 驱动                              │
│  - keys: 按键去抖 + 事件                        │
│  - battery: ADC 电量                            │
│  - rtc: DS3231                                  │
│  - sd_card: SPI SD 卡 + FATFS                  │
└─────────────────────────────────────────────────┘
```

### 3.2 依赖方向
- 上层依赖下层（UI → Engine → HAL）
- 跨层调用通过接口（*.h）解耦
- Engine 层不引用 UI/HAL 的具体实现
- HAL 层不引用 Engine/UI

---

## 4. 模块设计

### 4.1 HAL 层

| 模块 | 接口 | 说明 |
|------|------|------|
| `epd.c/h` | `epd_init / clear / set_pixel / draw_rect / fill_rect / display_full / display_partial / sleep / set_rotation` | 直接操作 framebuffer，调用者需自行管理刷新 |
| `keys.c/h` | `keys_init(callback)` | 注册按键回调，回调签名 `void cb(KeyId, KeyEvent)` |
| `battery.c/h` | `battery_init / get_percent / is_charging / is_low / is_warning` | ADC 16 次采样均值 → SOC 查表 |
| `rtc.c/h` | `ds3231_init / get_time / set_time` | I²C 设备，使用旧版 driver/i2c.h（ESP-IDF v6 EOL 警告） |
| `sd_card.c/h` | FATFS 挂载 | mount 到 `/sd`，书籍存 `/sd/books/`，字体存 `/sd/fonts/` |

### 4.2 Engine 层

#### config（配置持久化）
- SysConfig（ssid/password/weatherApiKey/weatherCity/sleepLayout/displayRotation/sleepTimeoutSec/initialized）
- ReaderConfig（fontSize/fontId/lineSpacing/charSpacing/margin）
- 存储：ESP-IDF NVS 库，namespace = "sys" / "reader"
- 接口：`config_init / load_sys / save_sys / load_reader / save_reader / factory_reset`

#### event_bus（事件总线）
- 观察者模式，类型化事件
- EventType：`EVT_KEY / EVT_PAGE_SWITCH / EVT_CONFIG_CHANGED / EVT_WIFI_STATUS / EVT_BATTERY / EVT_SLEEP / EVT_WAKE`
- 简单实现：定长订阅者数组（无锁/单生产者）
- 当前订阅者：status_bar（订阅 EVT_BATTERY/EVT_WIFI_STATUS）

#### typesetter（FreeType 排版）
- 集成 ESP-IDF freetype 组件（v2.14.2）
- 字体从 SD 卡按需加载到 PSRAM（8MB PSRAM 充足）
- 预分页：按字符宽度累积 + 断行
- 渲染：单色位图 blit 到 EPD 帧缓冲
- 3 个字体槽：思源宋体/黑体、霞鹜文楷
- API：`init / register_font / paginate / render_page / free_pages / reload_config`

#### book_parser（书籍解析）
- 格式检测：扩展名 + ZIP 魔数（PK\x03\x04）
- TXT：UTF-8 BOM 跳过 + 启发式 GBK 检测 + 简化 GBK→UTF-8 转换
- EPUB：自写最小 stored-only ZIP reader + cJSON 解析 container.xml/OPF + HTML 标签剥离状态机
- 元数据：书名从文件名提取，bookId 用 FNV-1a 哈希
- API：`detect_format / load_text / extract_metadata / free_text`

### 4.3 UI 层

#### page_mgr（页面状态机）
- 当前实现：注册表 + 切换（switch）
- 计划扩展：push/pop 栈式导航（待实现）
- 页面接口（PageVtbl）：`on_enter / on_exit / on_key / on_render`
- 切换流程：调用上一页 on_exit → 切换 current_page → 调用 on_enter → 调用 on_render → epd_display_full

#### widget（点阵字体）
- 16×16 像素，1 bit/pixel，行优先高位在前
- 95 ASCII 可打印字符 + 16 个图标
- 字模生成：`scripts/generate_ui_font.py`（从 macOS STHeiti 提取 ASCII + 手绘图标）
- 字模嵌入：编译进 Flash（`src/ui/font_data.h`，3552 字节）
- 图标：电量 4 档 / WiFi 2 态 / 充电 / 天气 3 种 / 书 / 时钟 / 箭头 / 复选
- API：`init / draw_char / draw_text / text_width / draw_icon`

#### status_bar（全局状态栏）
- 20px 高，黑底白字
- 左：HH:MM 时间（ds3231_get_time）
- 右：WiFi 图标 + 充电图标 + 百分比
- 电量 < 20% 用 EPD_COLOR_RED
- 订阅 EVT_BATTERY/EVT_WIFI_STATUS 事件触发刷新
- esp_timer 60s 周期刷新时间

#### 4 个页面

| 页面 | PageId | 功能 | 按键 |
|------|--------|------|------|
| page_home | PAGE_HOME | 日期 + 天气占位 + 统计 | NEXT→书架 HOME→设置 |
| page_bookshelf | PAGE_BOOKSHELF | 3×2 网格 + 默认封面 + 翻页 | PREV/NEXT 移动/翻页 HOME→阅读 |
| page_reader | PAGE_READER | 加载书籍 + 排版渲染 + 翻页 | PREV/NEXT 短按翻页 长按跳 5 页 HOME→书架 |
| page_settings | PAGE_SETTINGS | 阅读排版 + 系统设置 | HOME 切子菜单 长按切换值 PWR→主页 |

---

## 5. 关键设计决策

### 5.1 字模存储：编译进 Flash（vs SPIFFS）
- **决策**：把 `ui_font_16.bin` 转换为 C 数组 (`font_data.h`) 编译进 Flash
- **原计划**：从 SPIFFS 加载
- **原因**：
  1. PlatformIO 6 + ESP-IDF 6 的 EMBED_FILES 路径解析异常
  2. 减少运行时依赖（无 SPIFFS 挂载开销）
  3. 启动更快（无文件系统 I/O）
- **代价**：增加 3.5KB Flash 占用

### 5.2 排版引擎：FreeType（vs 自写栅格化）
- **决策**：集成 ESP-IDF freetype 组件（2.14.2）
- **原因**：支持 TTF 矢量字体，未来扩展字体/字号/字符集零成本
- **代价**：Flash 占用 +394KB（7.6% → 20.1%），仍可接受

### 5.3 EPUB 解析：自写 stored-only ZIP（vs miniz/zlib）
- **决策**：自写最小 stored-only ZIP reader
- **原因**：ESP-IDF component manager 中无现成 ZIP 库（esp-miniz/esp-zlib 不可用）
- **限制**：仅支持 stored（未压缩）的 EPUB 条目
- **替代方案**：完整支持需集成 miniz（约 30KB Flash + 50KB RAM）

### 5.4 GBK→UTF-8：区间映射（vs 精确查表）
- **决策**：用 `(c1-0x81)*191 + (c2-0x40)` 区间映射
- **原因**：查表 21K 项占 60KB Flash，不适合资源受限
- **限制**：汉字 Unicode 码点可能错位（视觉字符不一致）
- **生产替代**：用 iconv 或 ICU 库

### 5.5 链接器 GC：UI 注册触发链接
- **问题**：未在 main.c 调用的函数会被 ESP-IDF 的 `--gc-sections` 清除
- **解决**：所有 page_vtbl 必须在 main.c 注册（page_mgr_register）
- **结果**：page_reader 引用 book_parser/typesetter，触发整条调用链链接

---

## 6. 数据流

### 6.1 启动序列
```
app_main()
  → nvs_flash_init()                    // NVS
  → config_init()                       // 加载默认配置
  → event_bus_init()                    // 事件总线
  → epd_init()                          // 初始化 SPI + framebuffer
  → widget_init()                       // 准备字模
  → page_mgr_init()                     // 页面状态机
  → status_bar_init()                   // 订阅事件 + 启动 60s 定时器
  → typesetter_register_font() ×3       // 注册字体路径
  → typesetter_init()                   // FT_Init_FreeType
  → page_mgr_register(home/bookshelf/reader/settings)
  → keys_init(page_mgr_handle_key)      // 按键分发
  → page_mgr_switch(PAGE_HOME)          // 进入主页 → on_render → epd_display_full
  → 主循环（空转，等待事件）
```

### 6.2 按键处理
```
按键中断
  → keys.c 硬件去抖 + 事件分类（短/长/超长/组合）
  → page_mgr_handle_key(key, event)
  → pages[current].on_key(key, event)
  → 页面响应：调用 page_mgr_switch / on_render / 修改状态
```

### 6.3 阅读流程
```
用户进入阅读器（HOME 键 from 书架）
  → page_reader.on_enter
  → book_load_text() 加载到 PSRAM
  → typesetter_paginate() 预分页（PageIndex 数组）
  → 渲染：on_render → typesetter_render_page → epd_set_pixel
  → epd_display_partial()（5 次后全刷）
翻页：
  → PREV/NEXT → 修改 s_current_page → on_render
退出：
  → HOME 键 → page_mgr_switch(PAGE_BOOKSHELF)
```

---

## 7. 内存与性能预算

### 7.1 Flash 占用（实际测量）
| 组件 | 占用 |
|------|------|
| FreeType 库 | ~390KB |
| ESP-IDF + 驱动 | ~150KB |
| 应用代码 + 字体 | ~120KB |
| **总计** | **660KB / 3MB（21.0%）** |

### 7.2 RAM 占用
- 静态分配：约 30KB（framebuffer 2×15KB + 配置 + 任务栈）
- 运行时动态：typesetter 字体缓冲（~4MB/字体，从 PSRAM 分配）

### 7.3 性能基准
- EPD 全刷：~15s（三色）
- EPD 局部刷新：~250ms（黑白）
- 启动到主页：~3s（含 SPI 初始化）
- 排版渲染（假设 1000 字/页）：< 500ms

---

## 8. 构建与烧录

### 8.1 构建命令
```bash
# 编译
pio run

# 烧录
pio run -t upload

# 监视串口
pio device monitor
```

### 8.2 目录结构
```
ai-bookreader/
├── platformio.ini          # PlatformIO 配置（ESP32-S3, ESP-IDF）
├── CMakeLists.txt          # 顶层 CMake（idf project）
├── default_16MB.csv        # Flash 分区表
├── src/                    # 源代码（ESP-IDF component）
│   ├── CMakeLists.txt      # idf_component_register
│   ├── idf_component.yml   # 依赖（freetype, cjson）
│   ├── main.c              # 入口
│   ├── hal/                # 硬件抽象层
│   ├── engine/             # 引擎层
│   ├── ui/                 # UI 层
│   ├── assets/fonts/       # 字模（编译进 flash）
│   └── font_data.h         # 字模 C 数组
├── data/                   # 资源（备份）
├── scripts/                # 工具脚本
└── docs/superpowers/plans/ # 实现计划
```

### 8.3 依赖（idf_component.yml）
- `espressif/freetype` (2.14.2)
- `espressif/cjson` (1.7.19~2)

---

## 9. 后续路线图

### Phase 3：休眠与基础服务
- [ ] 休眠页（3 种布局：时钟天气/风景图/诗词）
- [ ] WiFi 管理（STA/AP 切换 + 配网）
- [ ] 天气服务（和风天气 API）
- [ ] OTA 升级

### Phase 4：网页管理
- [ ] esp_http_server 路由
- [ ] 文件上传/删除（管理 /sd/books/）
- [ ] 配置修改接口

### Phase 5：优化与打磨
- [ ] 关闭 ADC 告警（升级到 driver/adc.h）
- [x] PSRAM 兼容（已使用 N16R8 型号，8MB PSRAM 可用）
- [x] 完整 ZIP 支持（已集成 ESP-IDF ROM miniz，支持 deflate 解压）
- [x] Flash 大小配置（sdkconfig 已更新为 16MB）
- [ ] GBK→UTF-8 精确查表（需集成 iconv）
- [ ] I2C 驱动迁移（rtc.c 从 driver/i2c.h 迁移到 driver/i2c_master.h）

---

## 10. 附录

### 10.1 已知问题
1. **~~无 PSRAM~~**：已解决。当前硬件为 ESP32-S3 N16R8，8MB PSRAM 可用。
2. **I2C 旧版驱动**：rtc.c 使用 `driver/i2c.h`（ESP-IDF v6 EOL），需迁移到 `driver/i2c_master.h`。
3. **~~page_mgr push/pop 未实现~~**：已解决。栈式导航已实现（push/pop/switch）。
4. **~~EPUB deflate 不支持~~**：已解决。已集成 ESP-IDF ROM miniz，支持 deflate 解压。
5. **GBK 映射精度**：汉字 Unicode 码点可能错位（视觉错误），需换 iconv。
6. **~~Flash 大小配置不一致~~**：已解决。sdkconfig 已更新为 16MB，与 platformio.ini 一致。

### 10.2 参考
- [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/)
- [FreeType 文档](https://freetype.org/freetype2/docs/)
- [PlatformIO ESP-IDF](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [SSD1683 数据手册](https://www.good-display.com/)
- 项目内部：`docs/superpowers/plans/2025-06-15-*.md`

---

**文档版本**：v0.3.1
**最后更新**：2026-06-16（技术债清理：Flash 配置修复）
**作者**：妙阅开发团队

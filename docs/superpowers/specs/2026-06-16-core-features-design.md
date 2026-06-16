# 核心功能设计规格

**日期**: 2026-06-16
**范围**: PC 仿真共享层 (app/shared/)
**目标**: 实现 6 个核心功能，让阅读器体验完整

---

## 1. 功能清单

| # | 功能 | 优先级 |
|---|------|--------|
| 1 | 真实书籍加载（TXT + EPUB） | P0 |
| 2 | 阅读进度持久化 | P0 |
| 3 | 设置页交互 | P0 |
| 4 | 阅读上下文菜单 | P0 |
| 5 | 书签功能 | P0 |
| 6 | EPUB 解析 | P0 |

**开发顺序**: 1 → 2 → 3 → 4 → 5 → 6

---

## 2. 架构改动

### 2.1 新增文件

```
app/shared/
├── engine/
│   ├── bookmark_mgr.h/c      书签管理
│   └── book_parser.h/c       书籍解析（TXT + EPUB）
├── storage/
│   └── book_storage.h/c      统一 JSON 存储
└── books/                     书籍文件目录（不入库）
    ├── 三体.txt
    └── 活着.txt
```

### 2.2 修改文件

- `shared/mock/mock_books.c` — 从本地目录扫描书籍
- `shared/ui/page_settings.c` — 交互逻辑
- `shared/ui/page_reader.c` — 上下文菜单 + 书签 + 进度
- `shared/ui/page_bookshelf.c` — 真实书籍列表
- `sim_main.c` — 集成初始化

---

## 3. 真实书籍加载

### 3.1 book_parser 接口

```c
typedef enum { BOOK_FMT_TXT, BOOK_FMT_EPUB, BOOK_FMT_UNKNOWN } BookFormat;

BookFormat book_parser_detect(const char *path);
char *book_parser_load_text(const char *path);  // 返回 malloc 的 UTF-8 字符串

typedef struct {
    char title[128];
    char author[64];
    int total_chars;
} BookMeta;

int book_parser_extract_meta(const char *path, BookMeta *meta);
```

### 3.2 TXT 解析

- 读取文件 → 检测 UTF-8 BOM → 跳过
- 启发式 GBK 检测（高位字节 0x81-0xFE）→ 区间映射转 UTF-8
- 返回纯文本字符串

### 3.3 EPUB 解析（stored ZIP）

1. 读取 ZIP 中央目录（EOCD → Central Directory）
2. 查找 `META-INF/container.xml` → 获取 OPF 路径
3. 解析 OPF → 获取 spine（阅读顺序）和 manifest
4. 按 spine 顺序提取 XHTML 文件
5. HTML 标签剥离状态机 → 纯文本
6. 拼接所有章节文本

### 3.4 mock_books 改造

- `mock_books_init()` 扫描 `app/books/` 目录
- 自动检测 .txt / .epub 文件
- 调用 `book_parser` 提取元数据
- `mock_books_get_text(index)` 按需加载文本

---

## 4. 阅读进度持久化

### 4.1 存储格式

文件：`app/data/reading_data.json`

```json
{
  "progress": {
    "三体.txt": { "page": 23, "timestamp": 1718520000 }
  },
  "bookmarks": {
    "三体.txt": [
      { "page": 10, "note": "", "timestamp": 1718520100 }
    ]
  }
}
```

### 4.2 book_storage 接口

```c
void book_storage_init(void);
void book_storage_save_progress(const char *book_name, int page);
int book_storage_load_progress(const char *book_name);  // 返回页码，-1=无记录
void book_storage_add_bookmark(const char *book_name, int page);
int book_storage_get_bookmarks(const char *book_name, int *pages, int max_count);
void book_storage_save(void);  // 写入文件
```

### 4.3 行为规则

- 每次翻页自动保存到内存
- 退出阅读 / 切换书时写入文件
- 打开书时自动跳转到上次位置
- 每本书最多 20 个书签，超出删除最早的

---

## 5. 设置页交互

### 5.1 状态机

```
BROWSE 模式（浏览）
  PREV/NEXT → 光标上下移动
  HOME → 进入 EDIT 模式

EDIT 模式（编辑）
  PREV/NEXT → 循环切换当前选项值
  HOME → 确认修改，回到 BROWSE
  PWR → 取消修改，回到 BROWSE
```

### 5.2 菜单项结构

```c
typedef struct {
    const char *label;
    const char **options;
    int option_count;
    int current_value;
} SettingItem;
```

### 5.3 设置项

| 分类 | 项目 | 可选值 |
|------|------|--------|
| 阅读排版 | 字号 | 16/18/20/22/24/28/32 px |
| 阅读排版 | 行间距 | 1.0/1.2/1.5/2.0 |
| 阅读排版 | 字间距 | 0/1/2/3/4 px |
| 阅读排版 | 页边距 | 4/8/12/16 px |
| 系统 | 自动休眠 | 5/10/30 分钟/不自动 |

### 5.4 视觉效果

```
┌──────────────────────────────────┐
│  状态栏                           │
├──────────────────────────────────┤
│  阅读排版                         │
│  ────────────────                 │
│ ▶ 字号      20px     ← 光标      │
│   行间距    1.5                   │
│   字间距    1px                   │
│   页边距    8px                   │
│  ────────────────                 │
│  系统设置                         │
│  ────────────────                 │
│   自动休眠  5分钟                 │
│   版本      v0.5.0               │
└──────────────────────────────────┘
```

编辑模式时当前值显示为红色。

---

## 6. 阅读上下文菜单

### 6.1 触发

阅读界面按 HOME 键弹出。

### 6.2 菜单选项

1. ▶ 继续阅读
2. ★ 添加书签
3. ℹ 书籍信息

### 6.3 交互

- PREV/NEXT 移动光标
- HOME 确认选中项
- PWR 关闭菜单（等同继续阅读）

### 6.4 视觉效果

```
┌──────────────────────────────────┐
│  第三章 深渊中的黑暗森林  🔋📶   │
├──────────────────────────────────┤
│  ┌────────────────────────────┐  │
│  │ ▶ 继续阅读          ←选中 │  │
│  │   ★ 添加书签              │  │
│  │   ℹ 书籍信息              │  │
│  └────────────────────────────┘  │
│                                  │
│  （正文内容，被菜单遮挡）         │
├──────────────────────────────────┤
│  第 23 页 / 共 312 页            │
└──────────────────────────────────┘
```

### 6.5 实现

page_reader.c 增加 `s_menu_visible` 状态，渲染时先画正文再画菜单覆盖层。

---

## 7. 书籍信息显示

HOME → ℹ 书籍信息，显示：
- 书名
- 总页数
- 当前页 / 进度百分比
- 书签数量

---

## 8. 不在范围内

- 字体切换（PC 仿真只有一种字体）
- 跳转到指定页码
- 搜索功能
- 目录导航

---

**文档版本**: v1.0
**状态**: 已批准，待实现

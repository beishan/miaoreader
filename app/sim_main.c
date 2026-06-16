/**
 * @file sim_main.c
 * @brief PC 仿真入口（使用共享页面层）
 *
 * 布局：状态栏（顶部）+ 主体区域（中间）+ 菜单栏（底部）
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include "shared/ui/page_mgr.h"
#include "shared/ui/page_home.h"
#include "shared/ui/page_bookshelf.h"
#include "shared/ui/page_reader.h"
#include "shared/ui/page_settings.h"
#include "shared/ui/status_bar.h"
#include "shared/ui/widget.h"
#include "shared/mock/mock_rtc.h"
#include "shared/mock/mock_weather.h"
#include "shared/mock/mock_books.h"
#include "shared/mock/mock_wifi.h"
#include "shared/storage/book_storage.h"
#include <stdio.h>
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
#define READER_MAX_PAGES 4  /* 每本书最大页数 */

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
        /* 阅读器：全屏显示 */
        renderer_fill_rect(0, CONTENT_Y, RENDERER_WIDTH, RENDERER_HEIGHT - CONTENT_Y, RENDERER_COLOR_WHITE);

        MockBookMeta book;
        mock_books_get_by_index(s_selected_book, &book);

        int line_h = 18;
        int text_y = CONTENT_Y + 5;
        int max_lines = (RENDERER_HEIGHT - text_y - 5) / line_h;  /* ~15 行 */

        /* 定义每本书的多页内容（每行 18-20 字，左右留 20px 边距） */
        #define MAX_PAGES READER_MAX_PAGES
        #define LINES_PER_PAGE 14

        /* 三体 - 4 页 */
        static const char *sansheng_pages[MAX_PAGES][LINES_PER_PAGE] = {
            { /* 第 1 页 */
                "汪淼没想到，这次会议的地点",
                "竟然是在一个地下掩体中。",
                "他顺着螺旋楼梯往下走了好",
                "一会儿，才到达一个宽敞的",
                "地下大厅。墙壁上挂满了各",
                "种显示屏，显示着世界各地",
                "的天文观测数据。",
                "",
                "  \"汪教授，请坐。\"一个穿",
                "军装的中年人走过来，\"我是",
                "常伟思，这次会议的主持人。\"",
                "",
                "  汪淼握了握他的手，在指定",
                "的位置坐下。会议室里已经坐",
            },
            { /* 第 2 页 */
                "了十几个人，有科学家，也有",
                "军人。他们的表情都很严肃，",
                "气氛异常凝重。",
                "",
                "  \"各位，\"常伟思站在大厅",
                "中央，\"最近发生了一些事情，",
                "一些让我们无法理解的事情。\"",
                "",
                "  他停顿了一下，目光扫过每",
                "一个人。\"在过去的一年里，",
                "全球有三位顶尖的物理学家",
                "自杀身亡。\"",
                "",
                "  会议室里一片沉默。汪淼感",
            },
            { /* 第 3 页 */
                "到一阵寒意从脊背升起。三位",
                "物理学家，都是他认识的人。",
                "",
                "  \"他们都在自杀前销毁了自",
                "己的研究资料。\"常伟思继续",
                "说道，\"所有的笔记本、电脑",
                "硬盘，全部被彻底销毁。\"",
                "",
                "  \"这不可能是巧合。\"一位年",
                "长的物理学家说道，\"一定有",
                "什么东西，让他们感到了恐惧。\"",
                "",
                "  常伟思点了点头，\"所以我",
                "们需要你们来帮助我们解开",
            },
            { /* 第 4 页 */
                "这个谜团。\"",
                "",
                "  汪淼看着屏幕上闪烁的数据，",
                "心中充满了困惑。他是一名纳",
                "米材料学家，对于理论物理并",
                "不精通。",
                "",
                "  \"汪教授，\"常伟思走到他面",
                "前，\"你的同事丁仪推荐了你。",
                "他说，你是一个能够看到事物",
                "本质的人。\"",
                "",
                "  他翻开文件夹第一页，上面",
                "只有一行字——\"物理学不存在",
            },
        };

        /* 活着 - 4 页 */
        static const char *huozhe_pages[MAX_PAGES][LINES_PER_PAGE] = {
            { /* 第 1 页 */
                "我比现在年轻十岁的时候，",
                "获得了一个游手好闲的职业，",
                "去乡间收集民间歌谣。那一",
                "年的整个夏天，我如同一只",
                "乱飞的麻雀，在知了和阳光",
                "充斥的村庄之间穿行。",
                "",
                "  老人和一头老牛走在田埂上，",
                "老人粗哑却令人感动的嗓音，",
                "穿越了整片田野。他唱着——",
                "",
                "  \"皇帝招我做女婿，",
                "路远迢迢我不去。\"",
                "",
            },
            { /* 第 2 页 */
                "  老人黝黑的脸在阳光里笑得",
                "十分生动，脸上的皱纹欢乐地",
                "游动着，里面镶满了泥土。",
                "",
                "  \"我叫福贵，\"他说，\"从前",
                "嘛，我家可是这村里数一数二",
                "的大户。我爹常说，我们家以",
                "前有两百多亩地。后来嘛，",
                "都让我给败光了。\"",
                "",
                "  他说这话的时候，脸上带着",
                "一种奇怪的笑容，仿佛在讲",
                "一个别人的故事。",
                "",
            },
            { /* 第 3 页 */
                "\"我年轻的时候啊，是个混蛋。",
                "赌钱，逛窑子，什么都干。",
                "后来有一天，我把家里的地",
                "全输光了。\"",
                "",
                "  \"那可是两百多亩地啊！",
                "我爹气得吐血。从那以后，",
                "我们家就败落了。\"",
                "",
                "  老人说到这里，停了下来，",
                "用手背擦了擦眼睛。那头老牛",
                "转过头来，用湿润的眼睛看着",
                "他，仿佛在安慰他。",
                "",
            },
            { /* 第 4 页 */
                "  \"后来嘛，\"老人继续说道，",
                "\"我就开始种地了。以前是地",
                "主，现在成了佃农。我娘、",
                "我媳妇、我女儿、我儿子，",
                "一个一个地走了。\"",
                "",
                "  他的声音变得很轻。\"现在",
                "就剩我一个人了，还有这头",
                "老牛。它也老了，和我一样。\"",
                "",
                "  \"人是为了活着本身而活着",
                "的，而不是为了活着之外的",
                "任何事物而活着。\"",
                "",
            },
        };

        /* 1984 - 4 页 */
        static const char *yjbs_pages[MAX_PAGES][LINES_PER_PAGE] = {
            { /* 第 1 页 */
                "四月间，天气寒冷晴朗，钟敲",
                "了十三下。温斯顿·史密斯为",
                "了要躲寒风，紧缩着脖子，",
                "很快地溜进了胜利大厦的玻",
                "璃门，一阵沙土跟着他刮进",
                "了门。",
                "",
                "  门厅里有一股熬白菜和旧地",
                "席的气味。门厅的一头，有一",
                "张彩色的招贴画钉在墙上。",
                "画上是一张很大的面孔，浓黑",
                "的胡子，面容粗犷英俊。",
                "",
                "  温斯顿走向楼梯。用不着试",
            },
            { /* 第 2 页 */
                "电梯。他住在七层楼上，今年",
                "三十九岁，右脚踝上有一处",
                "静脉曲张溃疡。",
                "",
                "  每一层楼梯间都有那张招贴",
                "画对着你。那张画上的人有一",
                "种奇特的本领，不管你走到",
                "哪里，他的眼光总是跟着你。",
                "下面的文字说明是：",
                "老大哥在看着你。",
                "",
                "  公寓里弥漫着一股陈腐的气",
                "味。温斯顿打开了一盏昏暗的",
                "电灯，房间里的一切都显得灰",
            },
            { /* 第 3 页 */
                "暗而破旧。",
                "",
                "  他走到窗前，看着外面的街",
                "道。远处的建筑上，老大哥的",
                "巨幅画像俯视着这座城市。",
                "",
                "  他从口袋里掏出一个小本子，",
                "这是他偷偷买的。在思想警察",
                "无处不在的时代，写日记是一",
                "种危险的行为。但他需要记录，",
                "需要思考。",
                "",
                "  他翻开本子，用颤抖的手写",
                "道：\"致未来，或致过去——",
            },
            { /* 第 4 页 */
                "致一个思想自由、人们彼此不",
                "同、并非孤独生活的时代。\"",
                "",
                "  \"从一个千篇一律的时代，",
                "从一个孤独的时代，从一个老",
                "大哥的时代——向你致敬！\"",
                "",
                "  他停下笔，听着外面巡逻队",
                "的脚步声。在这个世界里，连",
                "思想都是犯罪。",
                "",
                "  温斯顿知道，他正在做的事情",
                "是极其危险的。如果被发现，",
                "他会彻底地消失，仿佛从未存",
                "在过。",
            },
        };

        /* 红楼梦 - 4 页 */
        static const char *hlm_pages[MAX_PAGES][LINES_PER_PAGE] = {
            { /* 第 1 页 */
                "满纸荒唐言，一把辛酸泪。",
                "都云作者痴，谁解其中味。",
                "",
                "  此开卷第一回也。作者自云",
                "曾历过一番梦幻之后，故将真",
                "事隐去，而借\"通灵\"说此",
                "《石头记》一书也。",
                "",
                "  今风尘碌碌，一事无成，",
                "忽念及当日所有之女子，一一",
                "细考较去，觉其行止见识皆出",
                "我之上，我堂堂须眉，诚不若",
                "彼裙钗。",
                "",
            },
            { /* 第 2 页 */
                "  当此日，欲将已往所赖天恩",
                "祖德，锦衣纨裤之时，背父兄",
                "教育之恩，负师友规训之德，",
                "以致今日一技无成、半生潦倒",
                "之罪，编述一集，以告天下。",
                "",
                "  虽今日之茅椽蓬牖，瓦灶绳",
                "床，亦未有妨我之襟怀笔墨者。",
                "又何妨用假语村言，敷演出一",
                "段故事来，亦可使闺阁昭传。",
                "",
                "  原来女娲氏炼石补天之时，",
                "于大荒山无稽崖练成顽石三万",
                "六千五百零一块。",
            },
            { /* 第 3 页 */
                "  娲皇氏只用了三万六千五百",
                "块，只单单剩了一块未用，便",
                "弃在此山青埂峰下。",
                "",
                "  谁知此石自经煅炼之后，灵",
                "性已通，因见众石俱得补天，",
                "独自己无材不堪入选，遂自怨",
                "自叹，日夜悲号惭愧。",
                "",
                "  一日，俄见一僧一道远远而",
                "来，生得骨格不凡，丰神迥异，",
                "说说笑笑来至峰下，坐于石边",
                "高谈快论。",
                "",
            },
            { /* 第 4 页 */
                "  此石听了，不觉打动凡心，",
                "也想要到人间去享一享这荣华",
                "富贵，便口吐人言，向那僧道",
                "说道：\"大师，弟子蠢物，不",
                "能见礼了。\"",
                "",
                "  那僧笑道：\"善哉！那红尘",
                "中有却有些乐事，但不能永远",
                "依恃。况又有'美中不足，好事",
                "多魔'八个字紧相连属。\"",
                "",
                "  石头听了，感谢不尽。那僧",
                "便念咒书符，大展幻术，将一",
                "块大石登时变成一块鲜明莹洁",
                "的美玉。",
            },
        };

        /* 通用内容 - 4 页 */
        static const char *default_pages[MAX_PAGES][LINES_PER_PAGE] = {
            { /* 第 1 页 */
                "这是一本值得一读的好书。",
                "每一本书都是一个世界，每一",
                "页都是一段旅程。当你翻开书",
                "页的那一刻，便踏上了一段奇",
                "妙的旅途。",
                "",
                "  文字的力量在于，它能带你",
                "穿越时空，感受不同的人生。",
                "在书中，你可以是勇敢的骑士，",
                "可以是智慧的学者，可以是平",
                "凡的百姓。",
                "",
                "  阅读是一种习惯，更是一种",
                "修行。每一次阅读，都是一次",
            },
            { /* 第 2 页 */
                "与作者的对话，一次与自己的",
                "对话。",
                "",
                "  在文字中，我们找到共鸣，",
                "找到力量，找到方向。书中的",
                "世界比现实更广阔，比梦境更",
                "真实。",
                "",
                "  古人云：书中自有黄金屋，",
                "书中自有颜如玉。千百年来，",
                "无数仁人志士在书中找到了人",
                "生的方向。",
                "",
                "  当你感到迷茫时，读一本好",
            },
            { /* 第 3 页 */
                "书；当你感到孤独时，读一本",
                "好书。阅读是最好的陪伴。",
                "",
                "  书是一盏灯，照亮前行的路。",
                "书是一面镜，映照真实的自己。",
                "书是一把钥匙，打开心灵的",
                "大门。",
                "",
                "  在这个喧嚣的世界里，找一",
                "本好书，泡一杯清茶，静静地",
                "读一个下午，便是最美好的时",
                "光。",
                "",
                "  让阅读成为一种习惯，让书",
            },
            { /* 第 4 页 */
                "香伴随你走过春夏秋冬。",
                "",
                "  读书不觉已春深，一寸光阴",
                "一寸金。古人读书，讲究的是",
                "心无旁骛，物我两忘。",
                "",
                "  愿你翻开每一页，都能收获",
                "新的感悟。愿你读完每一本，",
                "都能成为更好的自己。",
                "",
                "  书山有路勤为径，学海无涯",
                "苦作舟。与君共勉。",
                "",
                "  全文完。",
            },
        };

        /* 获取当前书的当前页内容 */
        const char **page_lines = NULL;
        int total_pages = MAX_PAGES;

        switch (s_selected_book) {
        case 0: page_lines = (const char **)sansheng_pages[s_reader_page]; break;
        case 2: page_lines = (const char **)huozhe_pages[s_reader_page]; break;
        case 3: page_lines = (const char **)yjbs_pages[s_reader_page]; break;
        case 4: page_lines = (const char **)hlm_pages[s_reader_page]; break;
        default: page_lines = (const char **)default_pages[s_reader_page]; break;
        }

        /* 渲染正文 */
        for (int i = 0; i < LINES_PER_PAGE && i < max_lines; i++) {
            if (page_lines[i][0] != '\0') {
                widget_draw_text(20, text_y + i * line_h, page_lines[i], RENDERER_COLOR_BLACK);
            }
        }

        /* 页码提示 */
        char page_hint[32];
        snprintf(page_hint, sizeof(page_hint), "< %d/%d >", s_reader_page + 1, total_pages);
        int hint_w = widget_text_width(page_hint);
        widget_draw_text((RENDERER_WIDTH - hint_w) / 2, RENDERER_HEIGHT - 20, page_hint, RENDERER_COLOR_BLACK);

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

                /* 书名（封面下方） */
                MockBookMeta book;
                if (mock_books_get_by_index(i, &book) == 0) {
                    int tw = widget_text_width(book.title);
                    int text_x = x + (cell_w - tw) / 2;
                    widget_draw_text(text_x, y + cover_h + 2, book.title, selected ? RENDERER_COLOR_RED : RENDERER_COLOR_BLACK);
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
    if (event != RENDERER_KEY_EVT_SHORT) return;

    /* 阅读器模式：ESC 返回书架，←/→ 翻页 */
    if (s_in_reader) {
        switch (key) {
        case RENDERER_KEY_PWR:
        case RENDERER_KEY_HOME:
            s_in_reader = false;
            s_in_bookshelf = true;
            s_current_menu = 1; /* 回到书架菜单 */
            break;
        case RENDERER_KEY_PREV:
            if (s_reader_page > 0) s_reader_page--;
            break;
        case RENDERER_KEY_NEXT:
            if (s_reader_page < READER_MAX_PAGES - 1) s_reader_page++;
            break;
        default:
            break;
        }
        custom_render();
        return;
    }

    /* 书架选书模式：←/→ 选书，ESC 退出，HOME 打开书 */
    if (s_in_bookshelf) {
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
            s_in_reader = true;
            s_reader_page = 0;  /* 重置页码 */
            break;
        default:
            break;
        }
        custom_render();
        return;
    }

    /* 标签模式：←/→ 切换标签，HOME 确认进入 */
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

    custom_render();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== 妙阅 E-Reader PC 仿真 ===\n\n");
    printf("按键:\n");
    printf("  ESC   - 退出选书模式 / 退出仿真\n");
    printf("  LEFT  - 上一个标签 / 上一本书\n");
    printf("  RIGHT - 下一个标签 / 下一本书\n");
    printf("  ENTER - 确认进入书架 / 打开书籍\n\n");

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
    page_mgr_register(&page_home_vtbl);
    page_mgr_register(&page_bookshelf_vtbl);
    page_mgr_register(&page_reader_vtbl);
    page_mgr_register(&page_settings_vtbl);

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

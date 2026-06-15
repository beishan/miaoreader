/**
 * @file page_sleep.c
 * @brief 休眠页面：时钟天气 / 风景图 / 诗词 三种布局
 */
#include "page_sleep.h"
#include "ui/widget.h"
#include "ui/status_bar.h"
#include "ui/page_mgr.h"
#include "hal/rtc.h"
#include "net/weather.h"
#include "engine/config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

static const char *TAG = "page_sleep";

/* 内置诗词库（50 首经典唐诗宋词） */
static const char *poems[][3] = {
    /* 唐诗 */
    {"静夜思", "李白", "床前明月光，疑是地上霜。\n举头望明月，低头思故乡。"},
    {"登鹳雀楼", "王之涣", "白日依山尽，黄河入海流。\n欲穷千里目，更上一层楼。"},
    {"春晓", "孟浩然", "春眠不觉晓，处处闻啼鸟。\n夜来风雨声，花落知多少。"},
    {"相思", "王维", "红豆生南国，春来发几枝。\n愿君多采撷，此物最相思。"},
    {"江雪", "柳宗元", "千山鸟飞绝，万径人踪灭。\n孤舟蓑笠翁，独钓寒江雪。"},
    {"枫桥夜泊", "张继", "月落乌啼霜满天，江枫渔火对愁眠。\n姑苏城外寒山寺，夜半钟声到客船。"},
    {"望庐山瀑布", "李白", "日照香炉生紫烟，遥看瀑布挂前川。\n飞流直下三千尺，疑是银河落九天。"},
    {"绝句", "杜甫", "两个黄鹂鸣翠柳，一行白鹭上青天。\n窗含西岭千秋雪，门泊东吴万里船。"},
    {"游子吟", "孟郊", "慈母手中线，游子身上衣。\n临行密密缝，意恐迟迟归。\n谁言寸草心，报得三春晖。"},
    {"悯农", "李绅", "锄禾日当午，汗滴禾下土。\n谁知盘中餐，粒粒皆辛苦。"},
    {"送元二使安西", "王维", "渭城朝雨浥轻尘，客舍青青柳色新。\n劝君更尽一杯酒，西出阳关无故人。"},
    {"九月九日忆山东兄弟", "王维", "独在异乡为异客，每逢佳节倍思亲。\n遥知兄弟登高处，遍插茱萸少一人。"},
    {"望天门山", "李白", "天门中断楚江开，碧水东流至此回。\n两岸青山相对出，孤帆一片日边来。"},
    {"早发白帝城", "李白", "朝辞白帝彩云间，千里江陵一日还。\n两岸猿声啼不住，轻舟已过万重山。"},
    {"望岳", "杜甫", "岱宗夫如何？齐鲁青未了。\n造化钟神秀，阴阳割昏晓。\n荡胸生曾云，决眦入归鸟。\n会当凌绝顶，一览众山小。"},
    {"春望", "杜甫", "国破山河在，城春草木深。\n感时花溅泪，恨别鸟惊心。\n烽火连三月，家书抵万金。\n白头搔更短，浑欲不胜簪。"},
    {"登高", "杜甫", "风急天高猿啸哀，渚清沙白鸟飞回。\n无边落木萧萧下，不尽长江滚滚来。"},
    {"黄鹤楼送孟浩然之广陵", "李白", "故人西辞黄鹤楼，烟花三月下扬州。\n孤帆远影碧空尽，唯见长江天际流。"},
    {"赠汪伦", "李白", "李白乘舟将欲行，忽闻岸上踏歌声。\n桃花潭水深千尺，不及汪伦送我情。"},
    {"出塞", "王昌龄", "秦时明月汉时关，万里长征人未还。\n但使龙城飞将在，不教胡马度阴山。"},
    {"芙蓉楼送辛渐", "王昌龄", "寒雨连江夜入吴，平明送客楚山孤。\n洛阳亲友如相问，一片冰心在玉壶。"},
    {"鹿柴", "王维", "空山不见人，但闻人语响。\n返景入深林，复照青苔上。"},
    {"竹里馆", "王维", "独坐幽篁里，弹琴复长啸。\n深林人不知，明月来相照。"},
    {"送杜少府之任蜀州", "王勃", "城阙辅三秦，风烟望五津。\n与君离别意，同是宦游人。\n海内存知己，天涯若比邻。"},
    {"登幽州台歌", "陈子昂", "前不见古人，后不见来者。\n念天地之悠悠，独怆然而涕下。"},
    {"回乡偶书", "贺知章", "少小离家老大回，乡音无改鬓毛衰。\n儿童相见不相识，笑问客从何处来。"},
    {"咏柳", "贺知章", "碧玉妆成一树高，万条垂下绿丝绦。\n不知细叶谁裁出，二月春风似剪刀。"},
    {"凉州词", "王翰", "葡萄美酒夜光杯，欲饮琵琶马上催。\n醉卧沙场君莫笑，古来征战几人回？"},
    {"别董大", "高适", "千里黄云白日曛，北风吹雁雪纷纷。\n莫愁前路无知己，天下谁人不识君。"},
    {"江南春", "杜牧", "千里莺啼绿映红，水村山郭酒旗风。\n南朝四百八十寺，多少楼台烟雨中。"},
    {"清明", "杜牧", "清明时节雨纷纷，路上行人欲断魂。\n借问酒家何处有？牧童遥指杏花村。"},
    {"秋夕", "杜牧", "银烛秋光冷画屏，轻罗小扇扑流萤。\n天阶夜色凉如水，卧看牵牛织女星。"},
    {"泊秦淮", "杜牧", "烟笼寒水月笼沙，夜泊秦淮近酒家。\n商女不知亡国恨，隔江犹唱后庭花。"},
    {"乐游原", "李商隐", "向晚意不适，驱车登古原。\n夕阳无限好，只是近黄昏。"},
    {"无题", "李商隐", "相见时难别亦难，东风无力百花残。\n春蚕到死丝方尽，蜡炬成灰泪始干。"},
    {"夜雨寄北", "李商隐", "君问归期未有期，巴山夜雨涨秋池。\n何当共剪西窗烛，却话巴山夜雨时。"},
    /* 宋词 */
    {"水调歌头", "苏轼", "明月几时有？把酒问青天。\n不知天上宫阙，今夕是何年。\n人有悲欢离合，月有阴晴圆缺，\n此事古难全。\n但愿人长久，千里共婵娟。"},
    {"念奴娇·赤壁怀古", "苏轼", "大江东去，浪淘尽，千古风流人物。\n故垒西边，人道是，三国周郎赤壁。\n乱石穿空，惊涛拍岸，卷起千堆雪。"},
    {"江城子·密州出猎", "苏轼", "老夫聊发少年狂，左牵黄，右擎苍。\n会挽雕弓如满月，西北望，射天狼。"},
    {"定风波", "苏轼", "莫听穿林打叶声，何妨吟啸且徐行。\n竹杖芒鞋轻胜马，谁怕？\n一蓑烟雨任平生。"},
    {"声声慢", "李清照", "寻寻觅觅，冷冷清清，凄凄惨惨戚戚。\n乍暖还寒时候，最难将息。\n三杯两盏淡酒，怎敌他、晚来风急？"},
    {"如梦令", "李清照", "昨夜雨疏风骤，浓睡不消残酒。\n试问卷帘人，却道海棠依旧。\n知否，知否？应是绿肥红瘦。"},
    {"一剪梅", "李清照", "红藕香残玉簟秋，轻解罗裳，独上兰舟。\n云中谁寄锦书来？雁字回时，月满西楼。\n花自飘零水自流，一种相思，两处闲愁。"},
    {"满江红", "岳飞", "怒发冲冠，凭栏处、潇潇雨歇。\n抬望眼、仰天长啸，壮怀激烈。\n三十功名尘与土，八千里路云和月。"},
    {"破阵子", "辛弃疾", "醉里挑灯看剑，梦回吹角连营。\n八百里分麾下炙，五十弦翻塞外声。\n沙场秋点兵。"},
    {"青玉案·元夕", "辛弃疾", "东风夜放花千树，更吹落、星如雨。\n众里寻他千百度，蓦然回首，\n那人却在，灯火阑珊处。"},
    {"虞美人", "李煜", "春花秋月何时了？往事知多少。\n小楼昨夜又东风，故国不堪回首月明中。\n问君能有几多愁？恰似一江春水向东流。"},
    {"相见欢", "李煜", "无言独上西楼，月如钩。\n寂寞梧桐深院锁清秋。\n剪不断，理还乱，是离愁。\n别是一般滋味在心头。"},
    {"浪淘沙令", "李煜", "帘外雨潺潺，春意阑珊。\n罗衾不耐五更寒。\n梦里不知身是客，一晌贪欢。"},
};

#define POEM_COUNT (sizeof(poems) / sizeof(poems[0]))

static const char *weekday_cn(int wday)
{
    static const char *days[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    if (wday < 0 || wday > 6) return "--";
    return days[wday];
}

/* 布局 0：时钟 + 天气 */
static void render_clock_weather(void)
{
    struct tm t;
    if (ds3231_get_time(&t) != ESP_OK) {
        memset(&t, 0, sizeof(t));
    }

    /* 大字体时间 HH:MM */
    char time_str[8];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t.tm_hour, t.tm_min);
    int tw = widget_text_width(time_str);
    /* 时间居中显示，稍微放大（用两倍绘制模拟大字体） */
    widget_draw_text((EPD_WIDTH - tw) / 2, 60, time_str, EPD_COLOR_BLACK);

    /* 日期 */
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%s %d月%d日",
             weekday_cn(t.tm_wday), t.tm_mon + 1, t.tm_mday);
    int dw = widget_text_width(date_str);
    widget_draw_text((EPD_WIDTH - dw) / 2, 100, date_str, EPD_COLOR_BLACK);

    /* 天气信息 */
    WeatherData weather;
    if (weather_get_current(&weather) == ESP_OK && weather.temp_now != 0) {
        char city_str[80];
        snprintf(city_str, sizeof(city_str), "%s %s", weather.city, weather.condition);
        int cw = widget_text_width(city_str);
        widget_draw_text((EPD_WIDTH - cw) / 2, 140, city_str, EPD_COLOR_BLACK);

        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%d°C", weather.temp_now);
        int ttw = widget_text_width(temp_str);
        widget_draw_text((EPD_WIDTH - ttw) / 2, 160, temp_str, EPD_COLOR_BLACK);
    }
}

/* 布局 1：风景图（简化版：显示默认图案） */
static void render_landscape(void)
{
    /* 绘制简单的山水图案 */
    /* 山峰轮廓 */
    for (int x = 0; x < EPD_WIDTH; x += 2) {
        int h = 150 + (x * 37 % 50) - 25;
        epd_draw_rect(x, h, 1, EPD_HEIGHT - h, EPD_COLOR_BLACK);
    }

    /* 太阳 */
    int sun_x = EPD_WIDTH - 80;
    int sun_y = 50;
    for (int r = 15; r < 20; r++) {
        for (int angle = 0; angle < 360; angle += 10) {
            int px = sun_x + (int)(r * cos(angle * 3.14159 / 180));
            int py = sun_y + (int)(r * sin(angle * 3.14159 / 180));
            if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT) {
                epd_set_pixel(px, py, EPD_COLOR_BLACK);
            }
        }
    }

    /* 底部文字 */
    const char *text = "妙阅 E-Reader";
    int tw = widget_text_width(text);
    widget_draw_text((EPD_WIDTH - tw) / 2, EPD_HEIGHT - 30, text, EPD_COLOR_BLACK);
}

/* 布局 2：诗词 + 时间 */
static void render_poem(void)
{
    struct tm t;
    if (ds3231_get_time(&t) != ESP_OK) {
        memset(&t, 0, sizeof(t));
    }

    /* 随机选择一首诗（基于当前分钟） */
    int idx = t.tm_min % POEM_COUNT;
    const char *title = poems[idx][0];
    const char *author = poems[idx][1];
    const char *content = poems[idx][2];

    /* 诗名 */
    int title_w = widget_text_width(title);
    widget_draw_text((EPD_WIDTH - title_w) / 2, 40, title, EPD_COLOR_BLACK);

    /* 作者 */
    char author_str[32];
    snprintf(author_str, sizeof(author_str), "——%s", author);
    int aw = widget_text_width(author_str);
    widget_draw_text((EPD_WIDTH - aw) / 2, 60, author_str, EPD_COLOR_BLACK);

    /* 诗句（逐行显示） */
    int y = 100;
    const char *line = content;
    while (*line) {
        const char *nl = strchr(line, '\n');
        int len = nl ? (int)(nl - line) : (int)strlen(line);

        char line_buf[64];
        if (len >= (int)sizeof(line_buf)) len = sizeof(line_buf) - 1;
        memcpy(line_buf, line, len);
        line_buf[len] = '\0';

        int lw = widget_text_width(line_buf);
        widget_draw_text((EPD_WIDTH - lw) / 2, y, line_buf, EPD_COLOR_BLACK);

        y += 20;
        line = nl ? nl + 1 : line + len;
    }

    /* 底部时间 */
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02d:%02d  %s",
             t.tm_hour, t.tm_min, weekday_cn(t.tm_wday));
    int tw = widget_text_width(time_str);
    widget_draw_text((EPD_WIDTH - tw) / 2, EPD_HEIGHT - 30, time_str, EPD_COLOR_BLACK);
}

static void on_enter(void)
{
    ESP_LOGI(TAG, "进入休眠页面");
}

static void on_render(void)
{
    epd_clear(EPD_COLOR_WHITE);

    /* 根据配置选择布局 */
    SysConfig cfg;
    config_load_sys(&cfg);

    switch (cfg.sleepLayout) {
    case 0:
        render_clock_weather();
        break;
    case 1:
        render_landscape();
        break;
    case 2:
        render_poem();
        break;
    default:
        render_clock_weather();
        break;
    }
}

static void on_key(KeyId key, KeyEvent event)
{
    /* 只响应 PWR 短按唤醒 */
    if (key == KEY_PWR && event == KEY_EVT_SHORT) {
        ESP_LOGI(TAG, "唤醒，返回主页");
        page_mgr_switch(PAGE_HOME);
    }
}

const PageVtbl page_sleep_vtbl = {
    .id = PAGE_SLEEP,
    .on_enter = on_enter,
    .on_exit = NULL,
    .on_key = on_key,
    .on_render = on_render,
};

#ifdef PC_SIMULATION

#include "page_sleep.h"
#include "page_mgr.h"
#include "renderer_if.h"
#include "widget.h"
#include "mock_rtc.h"
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

static void render_poem(void) {
    struct tm t;
    mock_rtc_get_time(&t);
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

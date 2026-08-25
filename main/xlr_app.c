#include "xlr_app.h"
#include "bsp_audio.h"
#include "bsp_display.h"
#include "xlr_logic.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>

LV_FONT_DECLARE(xlr_font_16);
LV_FONT_DECLARE(xlr_font_display_48);
LV_IMAGE_DECLARE(xlr_finger_guide);
LV_IMAGE_DECLARE(xlr_paper_texture);

#define SAMPLE_RATE 16000
#define AUDIO_CHUNK 320
#define PAPER 0xE9E1CF
#define INK 0x28231D
#define MUTED 0x6F6557
#define LINE 0xB9AA90
#define FOG 0xDDD2BC
#define OLD_PAPER 0xD8C7A8
#define PAPER_SHADOW 0xC5B28F
#define VERMILION 0xB7482F

typedef enum { PAGE_WELCOME, PAGE_KEYS, PAGE_ABOUT, PAGE_RECORD, PAGE_RECORDING, PAGE_NUMBERS,
               PAGE_RESULT } page_t;
static const char *TAG = "xlr_app";
static page_t s_page;
static lv_obj_t *s_scr, *s_body, *s_title, *s_mic_dot, *s_mic_ring;
static lv_timer_t *s_anim_timer, *s_number_repeat_timer;
static TaskHandle_t s_audio_task;
static volatile bool s_record_request, s_record_stop;
static bool s_audio_ready, s_voice_valid;
static uint32_t s_number = 1, s_numbers[3];
static int s_number_index, s_result_page, s_about_page;
static int s_repeat_direction, s_repeat_ticks;
static xlr_cast_t s_cast;

static void change_number(int direction, bool fast);

static void stop_number_repeat(void) {
    if (s_number_repeat_timer) {
        lv_timer_delete(s_number_repeat_timer);
        s_number_repeat_timer = NULL;
    }
}

static void number_repeat(lv_timer_t *timer) {
    s_repeat_ticks++;
    change_number(s_repeat_direction, true);
    if (s_repeat_ticks == 3) lv_timer_set_period(timer, 150);
    else if (s_repeat_ticks == 9) lv_timer_set_period(timer, 75);
}

static void begin_number_repeat(int direction) {
    stop_number_repeat();
    s_repeat_direction = direction;
    s_repeat_ticks = 0;
    change_number(direction, false);
    s_number_repeat_timer = lv_timer_create(number_repeat, 280, NULL);
}

static void style_label(lv_obj_t *obj, int width, lv_text_align_t align, uint32_t color) {
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(obj, &xlr_font_16, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(obj, align, 0);
    lv_obj_set_style_text_line_space(obj, 7, 0);
}

static void style_hint(lv_obj_t *obj) {
    style_label(obj, 232, LV_TEXT_ALIGN_CENTER, MUTED);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_line_space(obj, 0, 0);
}

static lv_obj_t *rule(lv_obj_t *parent, int x, int y, int width, uint32_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, 1);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static void paper_full_texture(lv_obj_t *parent) {
    lv_obj_t *texture = lv_image_create(parent);
    lv_image_set_src(texture, &xlr_paper_texture);
    lv_obj_set_pos(texture, 0, 0);
    lv_obj_set_style_image_opa(texture, 190, 0);
}

static void footer_paper(lv_obj_t *parent) {
    static const uint16_t fibres[][3] = {
        {10, 283, 7}, {32, 304, 4}, {61, 277, 10}, {93, 295, 5},
        {121, 281, 8}, {153, 309, 11}, {184, 285, 4}, {211, 301, 8},
        {225, 278, 5}
    };
    lv_obj_t *band = lv_obj_create(parent);
    lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(band, 0, 271);
    lv_obj_set_size(band, 240, 49);
    lv_obj_set_style_bg_color(band, lv_color_hex(OLD_PAPER), 0);
    lv_obj_set_style_border_width(band, 0, 0);
    lv_obj_set_style_pad_all(band, 0, 0);
    rule(parent, 0, 271, 240, PAPER_SHADOW);
    rule(parent, 16, 274, 208, LINE);
    for (unsigned i = 0; i < sizeof(fibres) / sizeof(fibres[0]); i++) {
        lv_obj_t *fibre = lv_obj_create(parent);
        lv_obj_set_pos(fibre, fibres[i][0], fibres[i][1]);
        lv_obj_set_size(fibre, fibres[i][2], 1);
        lv_obj_set_style_bg_color(fibre, lv_color_hex(i % 2 ? PAPER_SHADOW : MUTED), 0);
        lv_obj_set_style_bg_opa(fibre, i % 2 ? 45 : 26, 0);
        lv_obj_set_style_border_width(fibre, 0, 0);
        lv_obj_set_style_pad_all(fibre, 0, 0);
    }
}

static lv_obj_t *ink_title(lv_obj_t *parent, const char *text, int width, int x, int y,
                           lv_text_align_t align, const lv_font_t *font) {
    lv_obj_t *bleed = lv_label_create(parent);
    lv_obj_set_width(bleed, width);
    lv_label_set_long_mode(bleed, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(bleed, font, 0);
    lv_obj_set_style_text_color(bleed, lv_color_hex(0x655747), 0);
    lv_obj_set_style_text_opa(bleed, 72, 0);
    lv_obj_set_style_text_align(bleed, align, 0);
    lv_label_set_text(bleed, text);
    lv_obj_set_pos(bleed, x + 1, y + 1);

    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_width(title, width);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(title, font, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(INK), 0);
    lv_obj_set_style_text_align(title, align, 0);
    lv_label_set_text(title, text);
    lv_obj_set_pos(title, x, y);
    return title;
}

static lv_obj_t *paper_screen(const char *eyebrow) {
    if (s_anim_timer) { lv_timer_delete(s_anim_timer); s_anim_timer = NULL; }
    if (s_scr) lv_obj_delete(s_scr);
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(PAPER), 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    paper_full_texture(s_scr);
    footer_paper(s_scr);
    lv_obj_t *brand = lv_label_create(s_scr);
    style_label(brand, 208, LV_TEXT_ALIGN_CENTER, VERMILION);
    lv_label_set_text(brand, eyebrow);
    lv_obj_set_pos(brand, 16, 13);
    rule(s_scr, 72, 42, 96, LINE);
    return s_scr;
}

static lv_obj_t *card(void) {
    lv_obj_t *obj = lv_obj_create(s_scr);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, 13, 57);
    lv_obj_set_size(obj, 214, 210);
    lv_obj_set_style_radius(obj, 1, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(LINE), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_pad_all(obj, 13, 0);
    return obj;
}

static void render(const char *title, const char *body, const char *footer) {
    paper_screen(title);
    s_body = lv_label_create(s_scr);
    style_label(s_body, 188, LV_TEXT_ALIGN_LEFT, MUTED);
    lv_label_set_text(s_body, body);
    lv_obj_set_pos(s_body, 26, 68);
    lv_obj_t *hint = lv_label_create(s_scr);
    style_hint(hint);
    lv_label_set_text(hint, footer);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_screen_load(s_scr);
}

static void show_welcome(void) {
    s_page = PAGE_WELCOME;
    paper_screen("");
    lv_obj_t *eyebrow = lv_label_create(s_scr);
    style_label(eyebrow, 208, LV_TEXT_ALIGN_CENTER, VERMILION);
    lv_label_set_text(eyebrow, "一事一问");
    lv_obj_set_pos(eyebrow, 16, 58);

    lv_obj_t *name = ink_title(s_scr, "小六壬", 160, 40, 83,
                               LV_TEXT_ALIGN_CENTER, &xlr_font_display_48);
    lv_obj_set_style_text_letter_space(name, 1, 0);
    rule(s_scr, 82, 148, 76, INK);
    rule(s_scr, 104, 153, 32, VERMILION);

    lv_obj_t *motto = lv_label_create(s_scr);
    style_label(motto, 208, LV_TEXT_ALIGN_CENTER, MUTED);
    lv_label_set_text(motto, "心中有事，便问一卦");
    lv_obj_set_pos(motto, 16, 166);

    lv_obj_t *seal = lv_obj_create(s_scr);
    lv_obj_remove_flag(seal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(seal, 99, 204);
    lv_obj_set_size(seal, 42, 42);
    lv_obj_set_style_bg_opa(seal, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(seal, lv_color_hex(VERMILION), 0);
    lv_obj_set_style_border_width(seal, 2, 0);
    lv_obj_set_style_radius(seal, 1, 0);
    lv_obj_set_style_pad_all(seal, 5, 0);
    lv_obj_t *seal_text = lv_label_create(seal);
    style_label(seal_text, 28, LV_TEXT_ALIGN_CENTER, VERMILION);
    lv_label_set_text(seal_text, "问");
    lv_obj_center(seal_text);

    lv_obj_t *hint = lv_label_create(s_scr);
    style_hint(hint);
    lv_label_set_text(hint, "↑键位·●开始·↓介绍");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_screen_load(s_scr);
}

static void show_keys(void) {
    s_page = PAGE_KEYS;
    paper_screen("键位说明");

    lv_obj_t *power = lv_label_create(s_scr);
    style_label(power, 112, LV_TEXT_ALIGN_LEFT, INK);
    lv_label_set_text(power, "○ 开关 ←");
    lv_obj_set_pos(power, 0, 66);

    static const char *labels[] = {"↑ 上键  →", "↓ 下键  →", "● 确定  →"};
    static const int y[] = {66, 174, 283};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *key = lv_label_create(s_scr);
        style_label(key, 112, LV_TEXT_ALIGN_RIGHT, INK);
        lv_label_set_text(key, labels[i]);
        lv_obj_set_pos(key, 128, y[i]);
    }

    lv_obj_t *hint = lv_label_create(s_scr);
    style_hint(hint);
    lv_label_set_text(hint, "按确定返回");
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
    lv_screen_load(s_scr);
}

static void show_about(void) {
    static const char *titles[] = {"什么是小六壬", "六宫顺序", "使用原则"};
    static const char *bodies[] = {
        "按固定六宫顺序完成推演的传统占测方法。\n\n一次只问一件具体的事。卦象只作参考，不替你决定。",
        "大安  →  留连  →  速喜\n\n赤口  →  小吉  →  空亡",
        "一事一问，问题具体。\n\n不反复起卦。\n\n结合现实判断。"
    };
    char footer[64];
    s_page = PAGE_ABOUT;
    if (s_about_page == 0) {
        paper_screen("为什么叫“掐指一算”");

        lv_obj_t *hand = lv_image_create(s_scr);
        lv_image_set_src(hand, &xlr_finger_guide);
        lv_image_set_scale(hand, 192);
        lv_obj_set_pos(hand, 60, 61);

        s_body = lv_label_create(s_scr);
        style_label(s_body, 208, LV_TEXT_ALIGN_CENTER, MUTED);
        lv_obj_set_style_text_line_space(s_body, 4, 0);
        lv_label_set_text(s_body, "六宫在左手有固定位置，\n沿着它们循环计数。 ");
        lv_obj_set_pos(s_body, 16, 184);

        lv_obj_t *hint = lv_label_create(s_scr);
        style_hint(hint);
        lv_label_set_text(hint, "↓继续·1/4·●返回");
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -15);
        lv_screen_load(s_scr);
        return;
    }
    lv_snprintf(footer, sizeof(footer), "↑↓翻页·%d/4·●返回", s_about_page + 1);
    render(titles[s_about_page - 1], bodies[s_about_page - 1], footer);
}

static void show_record(void) {
    s_page = PAGE_RECORD;
    paper_screen("问事");
    s_title = lv_label_create(s_scr);
    style_label(s_title, 208, LV_TEXT_ALIGN_CENTER, INK);
    lv_label_set_text(s_title, s_audio_ready ? "按住确定\n默念所问" : "麦克风不可用");
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, -8);
    rule(s_scr, 96, 203, 48, VERMILION);
    lv_screen_load(s_scr);
}

static void mic_anim(lv_timer_t *timer) {
    static int phase;
    (void)timer;
    phase = (phase + 1) % 12;
    int size = 66 + (phase < 6 ? phase : 12 - phase) * 3;
    if (s_mic_ring) {
        lv_obj_set_size(s_mic_ring, size, size);
        lv_obj_align(s_mic_ring, LV_ALIGN_TOP_MID, 0, 25);
    }
    if (s_mic_dot) lv_obj_set_style_opa(s_mic_dot, 150 + (phase % 6) * 17, 0);
}

static void show_recording(void) {
    s_page = PAGE_RECORDING;
    paper_screen("默念中");
    lv_obj_t *panel = card();
    s_mic_ring = lv_obj_create(panel);
    lv_obj_set_size(s_mic_ring, 66, 66);
    lv_obj_set_style_radius(s_mic_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_mic_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_mic_ring, lv_color_hex(0xC8C2B8), 0);
    lv_obj_set_style_border_width(s_mic_ring, 1, 0);
    lv_obj_align(s_mic_ring, LV_ALIGN_TOP_MID, 0, 25);
    s_mic_dot = lv_obj_create(s_mic_ring);
    lv_obj_set_size(s_mic_dot, 32, 32);
    lv_obj_set_style_radius(s_mic_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_mic_dot, lv_color_hex(0x242321), 0);
    lv_obj_set_style_border_width(s_mic_dot, 0, 0);
    lv_obj_center(s_mic_dot);
    s_body = lv_label_create(panel);
    style_label(s_body, 180, LV_TEXT_ALIGN_CENTER, INK);
    lv_label_set_text(s_body, "");
    lv_obj_align(s_body, LV_ALIGN_BOTTOM_MID, 0, -27);
    lv_obj_t *hint = lv_label_create(s_scr);
    style_hint(hint);
    lv_label_set_text(hint, "松开●·进入取数");
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 65);
    s_anim_timer = lv_timer_create(mic_anim, 70, NULL);
    lv_screen_load(s_scr);
}

static void show_number(void) {
    s_page = PAGE_NUMBERS;
    char eyebrow[40], number[4], progress[32];
    lv_snprintf(eyebrow,sizeof(eyebrow),"凭数起卦 · %d / 3",s_number_index+1);
    lv_snprintf(number,sizeof(number),"%lu",(unsigned long)s_number);
    if(s_number_index==0) lv_snprintf(progress,sizeof(progress),"—　—　—");
    else if(s_number_index==1) lv_snprintf(progress,sizeof(progress),"%lu　—　—",(unsigned long)s_numbers[0]);
    else lv_snprintf(progress,sizeof(progress),"%lu　%lu　—",(unsigned long)s_numbers[0],(unsigned long)s_numbers[1]);
    paper_screen(eyebrow);
    lv_obj_t *panel=card();
    lv_obj_t *num=lv_label_create(panel);
    lv_label_set_text(num,number);
    lv_obj_set_style_text_font(num,&lv_font_montserrat_48,0);
    lv_obj_set_style_text_color(num,lv_color_hex(INK),0);
    lv_obj_align(num,LV_ALIGN_CENTER,0,-22);
    lv_obj_t *up_arrow=lv_label_create(panel);
    style_label(up_arrow,30,LV_TEXT_ALIGN_CENTER,VERMILION);
    lv_label_set_text(up_arrow,"↑");
    lv_obj_align(up_arrow,LV_ALIGN_CENTER,0,-64);
    lv_obj_t *down_arrow=lv_label_create(panel);
    style_label(down_arrow,30,LV_TEXT_ALIGN_CENTER,VERMILION);
    lv_label_set_text(down_arrow,"↓");
    lv_obj_align(down_arrow,LV_ALIGN_CENTER,0,23);
    lv_obj_t *done=lv_label_create(panel);
    style_label(done,180,LV_TEXT_ALIGN_CENTER,MUTED);
    lv_label_set_text(done,progress);
    lv_obj_align(done,LV_ALIGN_BOTTOM_MID,0,-24);
    lv_obj_t *hint=lv_label_create(s_scr);
    style_hint(hint);
    lv_label_set_text(hint,"↑↓选数·●确定");
    lv_obj_align(hint,LV_ALIGN_BOTTOM_MID,0,-15);
    lv_screen_load(s_scr);
}

static const char *story_for(xlr_palace_t p, int page) {
    static const char *const TEXT[XLR_PALACE_COUNT][3] = {
        {"眼前的局面像一间门窗安稳的屋子。风声不大，事情也不会突然转向。你已有的基础，比想象中更可靠。", "接下来最重要的不是再添一把力，而是守住节奏。越想立刻看到变化，越可能打乱原本能成的路径。", "宜耐心等待、稳定推进、守住承诺；忌临时冒进、频繁改口。现实信息永远比卦象更重要。"},
        {"事情像一根没有解开的线，绕了一圈又回到原处。消息会迟，态度会反复，但这并不等于彻底失败。", "真正的阻力来自尚未说清的条件。越催促，线结越紧；先确认对方的限制与时间表，局面才会慢慢松动。", "宜缓进、多核实、等待变化；忌反复催逼、凭一次回应下结论。给事情一点时间。"},
        {"远处已经传来脚步声。消息、机会或转机可能比预想更快出现，眼前这段时间就是最有力量的窗口。", "这份快意需要你及时接住。若只是等待，好消息也可能从身边掠过；主动联系和明确下一步，会让机会真正落地。", "宜速办、主动沟通、抓住时机；忌拖延、犹豫不决。行动之前仍要核对事实。"},
        {"局面里藏着锋利的声音。一个措辞、一处误会，或利益分配中的细节，都可能让原本的小问题突然放大。", "事情并非一定做不成，但代价取决于沟通方式。先降温，再讨论事实；把重要信息留痕，比争一时输赢更有用。", "宜谨慎沟通、保留记录、核对细节；忌争辩、硬碰硬、冲动承诺。"},
        {"事情像一条不宽却平整的小路。没有惊人的跃升，却能一步一步走到一个真实、可握住的小结果。", "这时最适合把目标缩小，把合作做实。先完成眼前的一小步，后面的空间才会自然出现。", "宜合作、小步推进、求小成；忌过度乐观、把小利当成最终突破。"},
        {"原本期待的回声可能渐渐消失。计划、承诺或投入未必能得到对应结果，有些东西正在变轻，甚至离开。", "空并不只意味着失去；若你问的是麻烦何时结束，它也可能代表影响正在消散。先判断什么值得继续，什么应当放下。", "宜止损、重新确认、缓慢布局；忌无底线投入、把期待当成确定。重大决定请依靠专业意见。"}
    };
    return TEXT[p][page];
}

static void show_result(void) {
    char head[64], body[350], foot[80];
    xlr_palace_t final = s_cast.path[2];
    if (s_result_page == 0) {
        s_page = PAGE_RESULT;
        paper_screen("卦象 · 1 / 4");

        lv_obj_t *seal = lv_obj_create(s_scr);
        lv_obj_remove_flag(seal, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(seal, 28, 69);
        lv_obj_set_size(seal, 30, 48);
        lv_obj_set_style_bg_opa(seal, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(seal, lv_color_hex(VERMILION), 0);
        lv_obj_set_style_border_width(seal, 2, 0);
        lv_obj_set_style_radius(seal, 1, 0);
        lv_obj_set_style_pad_all(seal, 3, 0);
        lv_obj_t *seal_text = lv_label_create(seal);
        style_label(seal_text, 18, LV_TEXT_ALIGN_CENTER, VERMILION);
        lv_label_set_text(seal_text, "末\n宫");
        lv_obj_center(seal_text);

        ink_title(s_scr, xlr_palace_name(final), 152, 65, 70,
                  LV_TEXT_ALIGN_CENTER, &xlr_font_display_48);

        rule(s_scr, 50, 132, 140, INK);
        rule(s_scr, 76, 137, 88, VERMILION);

        lv_obj_t *core = lv_label_create(s_scr);
        style_label(core, 188, LV_TEXT_ALIGN_CENTER, INK);
        lv_label_set_text(core, xlr_palace_core(final));
        lv_obj_set_pos(core, 26, 151);

        lv_obj_t *path = lv_label_create(s_scr);
        style_label(path, 208, LV_TEXT_ALIGN_CENTER, MUTED);
        lv_snprintf(body, sizeof(body), "%lu · %lu · %lu\n\n%s  →  %s  →  %s",
                    (unsigned long)s_cast.input[0], (unsigned long)s_cast.input[1], (unsigned long)s_cast.input[2],
                    xlr_palace_name(s_cast.path[0]), xlr_palace_name(s_cast.path[1]), xlr_palace_name(final));
        lv_label_set_text(path, body);
        lv_obj_set_pos(path, 16, 191);

        lv_obj_t *hint = lv_label_create(s_scr);
        style_hint(hint);
        lv_label_set_text(hint, "↓读解析·长按●再问");
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -15);
        lv_screen_load(s_scr);
        return;
    } else {
        const char *sections[] = {"事情的开场", "接下来的变化", "给你的提醒"};
        lv_snprintf(head, sizeof(head), "%s", sections[s_result_page - 1]);
        lv_snprintf(body, sizeof(body), "%s", story_for(final, s_result_page - 1));
    }
    lv_snprintf(foot, sizeof(foot), "↑↓翻页·%d/4·长按●再问", s_result_page + 1);
    s_page = PAGE_RESULT;
    paper_screen("卦象解析");
    lv_obj_t *page_mark = lv_obj_create(s_scr);
    lv_obj_remove_flag(page_mark, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(page_mark, 18, 66);
    lv_obj_set_size(page_mark, 28, 28);
    lv_obj_set_style_bg_color(page_mark, lv_color_hex(VERMILION), 0);
    lv_obj_set_style_border_width(page_mark, 0, 0);
    lv_obj_set_style_radius(page_mark, 1, 0);
    lv_obj_t *page_text = lv_label_create(page_mark);
    style_label(page_text, 20, LV_TEXT_ALIGN_CENTER, PAPER);
    char page_num[4];
    lv_snprintf(page_num, sizeof(page_num), "%d", s_result_page);
    lv_label_set_text(page_text, page_num);
    lv_obj_center(page_text);

    s_title = ink_title(s_scr, head, 166, 56, 70, LV_TEXT_ALIGN_LEFT, &xlr_font_16);
    rule(s_scr, 18, 107, 204, INK);
    s_body = lv_label_create(s_scr);
    style_label(s_body, 198, LV_TEXT_ALIGN_LEFT, INK);
    lv_obj_set_style_text_line_space(s_body, 10, 0);
    lv_label_set_text(s_body, body);
    lv_obj_set_pos(s_body, 21, 124);

    lv_obj_t *hint = lv_label_create(s_scr);
    style_hint(hint);
    lv_label_set_text(hint, foot);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_screen_load(s_scr);
}

static void finish_recording(bool valid) {
    if (!bsp_lvgl_lock(500)) return;
    s_voice_valid = valid;
    s_number_index = 0; s_number = 1; show_number();
    bsp_lvgl_unlock();
}

static void record_question(void) {
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) { finish_recording(false); return; }
    int16_t *chunk = malloc(AUDIO_CHUNK * sizeof(*chunk));
    if (!chunk) { finish_recording(false); return; }
    uint64_t energy = 0;
    size_t samples = 0;
    while (!s_record_stop) {
        if (bsp_audio_read(chunk, AUDIO_CHUNK * sizeof(*chunk)) != ESP_OK) break;
        for (int i = 0; i < AUDIO_CHUNK; i++) { int32_t v = chunk[i]; energy += (uint32_t)(v < 0 ? -v : v); }
        samples += AUDIO_CHUNK;
    }
    free(chunk);
    uint32_t mean_abs = samples ? (uint32_t)(energy / samples) : 0;
    ESP_LOGI(TAG, "录音 samples=%u mean_abs=%lu", (unsigned)samples, (unsigned long)mean_abs);
    finish_recording(samples >= SAMPLE_RATE / 4 && mean_abs > 40);
}

static void audio_worker(void *arg) {
    (void)arg;
    for (;;) {
        if (s_record_request) { s_record_request = false; record_question(); }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void xlr_app_start(bool audio_ready) {
    s_audio_ready = audio_ready;
    if (!s_audio_task) xTaskCreate(audio_worker, "xlr_audio", 4096, NULL, 4, &s_audio_task);
    show_welcome();
}

static void change_number(int direction, bool fast) {
    (void)fast;
    if (direction > 0) s_number = s_number >= 9 ? 1 : s_number + 1;
    else s_number = s_number <= 1 ? 9 : s_number - 1;
    show_number();
}

void xlr_app_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (s_page == PAGE_RECORD && btn == BSP_BTN_OK && ev == BSP_BTN_PRESS && s_audio_ready) {
        s_record_stop = false; show_recording(); s_record_request = true; return;
    }
    if (s_page == PAGE_RECORDING && btn == BSP_BTN_OK && (ev == BSP_BTN_RELEASE || ev == BSP_BTN_CLICK)) {
        s_record_stop = true; return;
    }
    if (s_page == PAGE_NUMBERS && (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) && ev == BSP_BTN_PRESS) {
        begin_number_repeat(btn == BSP_BTN_UP ? 1 : -1);
        return;
    }
    if (s_page == PAGE_NUMBERS && (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) &&
        (ev == BSP_BTN_RELEASE || ev == BSP_BTN_CLICK)) {
        stop_number_repeat();
        return;
    }
    if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG && s_page != PAGE_RECORDING) {
        if (s_page == PAGE_RESULT) show_record();
        else show_welcome();
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    switch (s_page) {
    case PAGE_WELCOME:
        if (btn == BSP_BTN_UP) show_keys();
        else if (btn == BSP_BTN_DOWN) { s_about_page = 0; show_about(); }
        else if (btn == BSP_BTN_OK) show_record();
        break;
    case PAGE_KEYS:
        if (btn == BSP_BTN_OK) show_welcome();
        break;
    case PAGE_ABOUT:
        if (btn == BSP_BTN_UP && s_about_page > 0) { s_about_page--; show_about(); }
        else if (btn == BSP_BTN_DOWN && s_about_page < 3) { s_about_page++; show_about(); }
        else if (btn == BSP_BTN_OK) show_welcome();
        break;
    case PAGE_RECORD: break;
    case PAGE_RECORDING: break;
    case PAGE_NUMBERS:
        if (btn == BSP_BTN_UP) change_number(1, false);
        else if (btn == BSP_BTN_DOWN) change_number(-1, false);
        else if (btn == BSP_BTN_OK) {
            s_numbers[s_number_index++] = s_number; s_number = 1;
            if (s_number_index == 3) { xlr_cast_numbers(s_numbers[0], s_numbers[1], s_numbers[2], &s_cast); s_result_page = 0; show_result(); }
            else show_number();
        }
        break;
    case PAGE_RESULT:
        if (btn == BSP_BTN_DOWN && s_result_page < 3) { s_result_page++; show_result(); }
        else if (btn == BSP_BTN_UP && s_result_page > 0) { s_result_page--; show_result(); }
        break;
    }
}

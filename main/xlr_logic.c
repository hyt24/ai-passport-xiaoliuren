#include "xlr_logic.h"

#include <stddef.h>

static const char *const NAMES[XLR_PALACE_COUNT] = {
    "大安", "留连", "速喜", "赤口", "小吉", "空亡"
};

static const char *const CORES[XLR_PALACE_COUNT] = {
    "稳 · 吉", "拖 · 偏凶", "快 · 大吉", "冲 · 凶", "顺 · 小吉", "空 · 大凶"
};

static const char *const ADVICE[XLR_PALACE_COUNT] = {
    "事情宜稳不宜急。守住已有节奏，耐心推进，比贸然求快更有利。",
    "事情容易拖延反复。先核实消息，给变化留出时间，不要急着定论。",
    "好消息与机会来得较快。窗口就在眼前，宜主动行动并及时确认。",
    "沟通摩擦会放大代价。谨慎表达、留好记录，并反复核对细节。",
    "整体平顺，小步可成。适合合作与稳步推进，不宜期待一步登天。",
    "期待可能落空或逐渐消散。先收回过度投入，确认事情是否还有后续。"
};

xlr_palace_t xlr_count_from(xlr_palace_t start, uint32_t number) {
    if (start < 0 || start >= XLR_PALACE_COUNT || number == 0) return XLR_DA_AN;
    return (xlr_palace_t)(((uint32_t)start + ((number - 1U) % XLR_PALACE_COUNT)) %
                          XLR_PALACE_COUNT);
}

bool xlr_cast_numbers(uint32_t first, uint32_t second, uint32_t third,
                      xlr_cast_t *out) {
    if (!out || first == 0 || second == 0 || third == 0) return false;
    out->input[0] = first;
    out->input[1] = second;
    out->input[2] = third;
    xlr_palace_t current = XLR_DA_AN;
    for (int i = 0; i < 3; i++) {
        current = xlr_count_from(current, out->input[i]);
        out->path[i] = current;
    }
    return true;
}

const char *xlr_palace_name(xlr_palace_t palace) {
    return palace >= 0 && palace < XLR_PALACE_COUNT ? NAMES[palace] : "UNKNOWN";
}

const char *xlr_palace_core(xlr_palace_t palace) {
    return palace >= 0 && palace < XLR_PALACE_COUNT ? CORES[palace] : "UNKNOWN";
}

const char *xlr_palace_advice(xlr_palace_t palace) {
    return palace >= 0 && palace < XLR_PALACE_COUNT ? ADVICE[palace] : "No advice.";
}

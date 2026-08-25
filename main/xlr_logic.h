#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    XLR_DA_AN = 0,
    XLR_LIU_LIAN,
    XLR_SU_XI,
    XLR_CHI_KOU,
    XLR_XIAO_JI,
    XLR_KONG_WANG,
    XLR_PALACE_COUNT,
} xlr_palace_t;

typedef struct {
    uint32_t input[3];
    xlr_palace_t path[3];
} xlr_cast_t;

xlr_palace_t xlr_count_from(xlr_palace_t start, uint32_t number);
bool xlr_cast_numbers(uint32_t first, uint32_t second, uint32_t third,
                      xlr_cast_t *out);
const char *xlr_palace_name(xlr_palace_t palace);
const char *xlr_palace_core(xlr_palace_t palace);
const char *xlr_palace_advice(xlr_palace_t palace);


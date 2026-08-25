#pragma once
#include <stdbool.h>
typedef struct { int solar_year,solar_month,solar_day,hour,minute; int lunar_month,lunar_day; bool leap; int shichen; } xlr_moment_t;
bool xlr_calendar_now(xlr_moment_t *out);
const char *xlr_calendar_month_name(int month);
const char *xlr_calendar_day_name(int day);
const char *xlr_calendar_shichen_name(int number);

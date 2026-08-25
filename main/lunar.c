#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lunar.h"
#ifndef _WIN32
#define sprintf_s snprintf
#endif

const double g_pi = 3.1415926535898;
const double g_tau = 2 * g_pi;
const double g_degrees_to_radians = g_pi / 180.0;
const char* g_branches[] = {
    "申", "酉", "戌", "亥", "子", "丑", "寅", "卯", "辰", "巳", "午", "未"
};
const char* g_stems[] = {
    "庚", "辛", "壬", "癸", "甲", "乙", "丙", "丁", "戊", "己"
};

#ifdef LUNARH_BUILD_CLI
int main(int argc, char** argv)
{
    struct ArgumentList argl = lunarh_parse_argv(argc, argv);
    if (argl.valid == 0)
    {
        printf("Invalid arguments\n");
        return 1;
    }
    if (argl.task == LUNARH_GREGORIAN_TO_LUNAR_IN_CHINESE)
    {
        lunarh_display_Lunar_in_Chinese(argl);
    }
    else if (argl.task == LUNARH_GREGORIAN_TO_LUNAR_IN_MODERN_NOTATION)
    {
        lunarh_display_Lunar_in_modern_notation(argl);
    }
    else if (argl.task == LUNARH_LUNAR_TO_GREGORIAN)
    {
        lunarh_display_Gregorian(argl);
    }
    else if (argl.task == LUNARH_LUNAR_CALENDAR_OF_A_YEAR)
    {
        lunarh_display_Lunar_calendar_of_a_year(argl);
    }
    else
    {
        printf("Unrecognizable task: %d\n", argl.task);
        return 1;
    }
}
#endif

struct Lunar lunarh_convert_Gregorian_to_Lunar(struct Gregorian date)
{
    if (!date.valid)
    {
        struct Lunar lunar = { .valid = 0 };
        return lunar;
    }
    int year = date.year;
    int jd = date.jd;
    double tz = date.tz;
    int k = (int)floor((jd - 2415021.076998695) / 29.530588853);
    int month_start_jd = lunarh_get_kth_new_moon_jd(k + 1, tz);
    int next_month_start_jd;
    if (month_start_jd > jd)
    {
        next_month_start_jd = month_start_jd;
        month_start_jd = lunarh_get_kth_new_moon_jd(k, tz);
    }
    else
    {
        next_month_start_jd = lunarh_get_kth_new_moon_jd(k + 2, tz);
    }
    int a = lunarh_get_lunar_month_11_start_jd(year, tz);
    int b = a;
    int lunar_year;
    if (a >= month_start_jd)
    {
        lunar_year = year;
        a = lunarh_get_lunar_month_11_start_jd(year - 1, tz);
    }
    else
    {
        lunar_year = year + 1;
        b = lunarh_get_lunar_month_11_start_jd(year + 1, tz);
    }
    int lunar_day = jd - month_start_jd + 1;
    int diff = (int)floor((month_start_jd - a) / 29);
    int lunar_leap = 0;
    int lunar_month = diff + 11;
    int offset;
    if (b - a > 365)
    {
        offset = lunarh_get_leap_month_offset(a, tz);
        if (diff >= offset)
        {
            lunar_month = diff + 10;
            if (diff == offset)
            {
                lunar_leap = 1;
            }
        }
    }
    if (lunar_month > 12)
    {
        lunar_month -= 12;
    }
    if (lunar_month >= 11 && diff < 4)
    {
        lunar_year -= 1;
    }
    struct Lunar lunar = {
        .year = lunar_year,
        .month = lunar_month,
        .leap = lunar_leap,
        .month_size = next_month_start_jd - month_start_jd,
        .day = lunar_day,
        .jd = jd,
        .tz = tz,
        .valid = 1
    };
    return lunar;
}

struct Gregorian lunarh_convert_Lunar_to_Gregorian(struct Lunar lunar)
{
    if (!lunar.valid)
    {
        struct Gregorian date = { .valid = 0 };
        return date;
    }
    struct Gregorian date = lunarh_new_Gregorian_2(lunar.jd, lunar.tz);
    return date;
}

void lunarh_display_Lunar_in_Chinese(struct ArgumentList argl)
{
    struct Gregorian date = lunarh_new_Gregorian(argl.year, argl.month, argl.day, 
        argl.tz);
    if (!date.valid)
    {
        printf("Invalid Gregorian date: %04d.%02d.%02d\n", argl.year, 
            argl.month, argl.day);
        exit(1);
    }
    struct Lunar lunar = lunarh_convert_Gregorian_to_Lunar(date);
    char out[256];
    lunarh_stringify_Lunar_date(lunar, out, 256, argl.show_month_size);
    printf("%s\n", out);
}

void lunarh_display_Lunar_in_modern_notation(struct ArgumentList argl)
{
    struct Gregorian date = lunarh_new_Gregorian(argl.year, argl.month, argl.day, 
        argl.tz);
    if (!date.valid)
    {
        printf("Invalid Gregorian date: %04d.%02d.%02d\n", argl.year, 
            argl.month, argl.day);
        exit(1);
    }
    struct Lunar lunar = lunarh_convert_Gregorian_to_Lunar(date);
    printf("%04d.%02d%s.%02d\n", lunar.year, lunar.month, 
        lunar.leap ? "+" : "", lunar.day);
}

void lunarh_display_Gregorian(struct ArgumentList argl)
{
    struct Lunar lunar = lunarh_new_Lunar(argl.year, argl.month, argl.leap, argl.day,
        argl.tz);
    if (!lunar.valid)
    {
        printf("Invalid Lunar date: %04d.%02d%s.%02d\n", argl.year, 
            argl.month, argl.leap ? "+" : "", argl.day);
        exit(1);
    }
    struct Gregorian date = lunarh_convert_Lunar_to_Gregorian(lunar);
    printf("%04d.%02d.%02d\n", date.year, date.month, date.day);
}

void lunarh_display_Lunar_calendar_of_a_year(struct ArgumentList argl)
{
    double tz = argl.tz;
    int year = argl.year;
    int last_jd = INT_MAX;
    printf("Lunar\tGregorian\n");
    for (int month = 1; month <= 12; month += 1)
    {
        struct Lunar lunar = lunarh_new_Lunar(year, month, 0, 1, tz);
        if (lunar.jd - last_jd > 30)
        {
            struct Lunar lunar = lunarh_new_Lunar(year, month - 1, 1, 1, tz);
            struct Gregorian date = lunarh_convert_Lunar_to_Gregorian(lunar);
            printf("%04d.%02d+.01\t%04d.%02d.%02d\n",
                lunar.year, lunar.month, date.year, date.month, date.day);
        }
        struct Gregorian date = lunarh_convert_Lunar_to_Gregorian(lunar);
        printf("%04d.%02d.01\t%04d.%02d.%02d\n",
            lunar.year, lunar.month, date.year, date.month, date.day);
        last_jd = lunar.jd;
    }
}

const char *lunarh_get_branch_of_year(int year)
{
    int branch = year % 12;
    return g_branches[branch < 0 ? branch + 12 : branch];
}

int lunarh_get_kth_new_moon_jd(int k, double tz)
{
    double t = k / 1236.85;
    double t2 = t * t;
    double t3 = t2 * t;
    double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * t2 - 
        0.000000155 * t3 + 0.00033 * sin(
            (166.56 + 132.87 * t - 0.009173 * t2) * g_degrees_to_radians
        );
    double m = 359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3;
    double mpr = 306.0253 + 385.81691806 * k + 0.0107306 * t2 + 
        0.00001236 * t3;
    double f = 21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3;
    double c1 = (0.1734 - 0.000393 * t) * sin(m * g_degrees_to_radians) + 
        0.0021 * sin(2 * m * g_degrees_to_radians) - 
        0.4068 * sin(mpr * g_degrees_to_radians) + 
        0.0161 * sin(2 * mpr * g_degrees_to_radians) -
        0.0004 * sin(3 * mpr * g_degrees_to_radians) + 
        0.0104 * sin(2 * f * g_degrees_to_radians) - 
        0.0051 * sin((m + mpr) * g_degrees_to_radians) - 
        0.0074 * sin((m - mpr) * g_degrees_to_radians) + 
        0.0004 * sin((2 * f + m) * g_degrees_to_radians) - 
        0.0004 * sin((2 * f - m) * g_degrees_to_radians) - 
        0.0006 * sin((2 * f + mpr) * g_degrees_to_radians) + 
        0.0010 * sin((2 * f - mpr) * g_degrees_to_radians) + 
        0.0005 * sin((2 * mpr + m) * g_degrees_to_radians);
    double delta_t;
    if (t < -11)
    {
        delta_t = 0.001 + 0.000839 * t + 0.0002261 * t2 - 0.00000845 * t3 - 
            0.000000081 * t3 * t;
    }
    else
    {
        delta_t = -0.000278 + 0.000265 * t + 0.000262 * t2;
    }
    double jd = jd1 + c1 - delta_t;
    return (int)floor(jd + 0.5 + tz / 24);
}

int lunarh_get_leap_month_offset(int new_moon_11_jd, double tz)
{
    int k = floor((new_moon_11_jd - 2415021.076998695) / 29.530588853 + 0.5);
    int last = 0;
    int i = 1;
    int arc = lunarh_get_sun_longitude(lunarh_get_kth_new_moon_jd(k + 1, tz), tz);
    while (1)
    {
        last = arc;
        i += 1;
        arc = lunarh_get_sun_longitude(lunarh_get_kth_new_moon_jd(k + i, tz), tz);
        if (arc == last || i >= 14)
        {
            break;
        }
    }
    return i - 1;
}

int lunarh_get_lunar_month_11_start_jd(int year, double tz)
{
    struct Gregorian date = lunarh_new_Gregorian(year, 12, 31, tz);
    int offset = date.jd - 2415021;
    int k = floor(offset / 29.530588853);
    int new_moon_jd = lunarh_get_kth_new_moon_jd(k, tz);
    int sun_longitude = lunarh_get_sun_longitude(new_moon_jd, tz);
    if (sun_longitude >= 9) {
        return lunarh_get_kth_new_moon_jd(k - 1, tz);
    }
    return new_moon_jd;
}

int lunarh_is_leap_Gregorian_year(int year)
{
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

const char* lunarh_get_Chinese_digit(int value)
{
    switch (value)
    {
    case 1:
        return "一";
    case 2: 
        return "二";
    case 3:
        return "三";
    case 4:
        return "四";
    case 5:
        return "五";
    case 6:
        return "六";
    case 7:
        return "七";
    case 8:
        return "八";
    case 9:
        return "九";
    case 10:
        return "十";
    case 20:
        return "廿";
    case 30:
        return "三十";
    default:
        return "";
    }
}

int lunarh_get_num_days_of_Gregorian_month(int year, int month)
{
    switch (month)
    {
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return lunarh_is_leap_Gregorian_year(year) ? 29 : 28;
    default:
        return 31;
    }
}

const char *lunarh_get_stem_of_year(int value)
{
    int stem = value % 10;
    return g_stems[stem < 0 ? stem + 10 : stem];
}

int lunarh_get_sun_longitude(int jd, double tz)
{
    double t = (jd - 2451545.5 - tz / 24.0) / 36525;
    double t2 = t * t;
    double m = 357.52910 + 35999.05030 * t - 0.0001559 * t2 - 
        0.00000048 * t2 * t;
    double long0 = 280.46645 + 36000.76983 * t + 0.0003032 * t2;
    double dl = 
        (1.914600 - 0.004817 * t - 0.000014 * t2) * sin(
            m * g_degrees_to_radians
        ) + 
        (0.019993 - 0.000101 * t) * sin(2 * m * g_degrees_to_radians) + 
        0.000290 * sin(3 * m * g_degrees_to_radians);
    double longitude = (long0 + dl) * g_degrees_to_radians;
    longitude -= g_tau * floor(longitude / g_tau);
    return (int)floor(longitude / g_pi * 6);
}

int lunarh_is_valid_Gregorian(int year, int month, int day)
{
    if (month < 1 || month > 12)
    {
        return 0;
    }
    return day >= 1 && day <= lunarh_get_num_days_of_Gregorian_month(year, month);
}

struct Gregorian lunarh_new_Gregorian(int year, int month, int day, double tz)
{
    if (!lunarh_is_valid_Gregorian(year, month, day))
    {
        struct Gregorian date = { .valid = 0 };
        return date;
    }
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    int jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 
        - 32045;
    struct Gregorian date = { 
        .year = year, .month = month, .day = day, .jd = jd, .tz = tz, 
        .valid = 1
    };
    return date;
}

struct Gregorian lunarh_new_Gregorian_2(int jd, double tz)
{
    int a = jd + 32044;
    int b = (4 * a + 3) / 146097;
    int c = a - (b * 146097) / 4;
    int d = (4 * c + 3) / 1461;
    int e = c - (1461 * d) / 4;
    int m = (5 * e + 2) / 153;
    int day = e - (153 * m + 2) / 5 + 1;
    int month = m + 3 - 12 * (m / 10);
    int year = b * 100 + d - 4800 + m / 10;
    struct Gregorian date = { 
        .year = year, .month = month, .day = day, .jd = jd, .tz = tz, 
        .valid = 1 
    };
    return date;
}

struct Lunar lunarh_new_Lunar(int year, int month, int leap, int day, double tz)
{
    struct Lunar lunar = { .valid = 0 };
    if (month < 1 || month > 12 || day < 1 || day > 30)
    {
        return lunar;
    }
    int a;
    int b;
    if (month < 11)
    {
        a = lunarh_get_lunar_month_11_start_jd(year - 1, tz);
        b = lunarh_get_lunar_month_11_start_jd(year, tz);
    }
    else 
    {
        a = lunarh_get_lunar_month_11_start_jd(year, tz);
        b = lunarh_get_lunar_month_11_start_jd(year + 1, tz);
    }
    int offset = month - 11;
    if (offset < 0)
    {
        offset += 12;
    }
    if (b - a > 365)
    {
        int leap_month_offset = lunarh_get_leap_month_offset(a, tz);
        int leap_month = leap_month_offset - 2;
        if (leap_month < 0)
        {
            leap_month += 12;
        }
        if (leap != 0 && month != leap_month)
        {
            return lunar;
        }
        else if (leap != 0 || offset >= leap_month_offset)
        {
            offset += 1;
        }
    }
    int k = (int)floor(0.5 + (a - 2415021.076998695) / 29.530588853);
    int month_start_jd = lunarh_get_kth_new_moon_jd(k + offset, tz);
    int next_month_start_jd = lunarh_get_kth_new_moon_jd(k + offset + 1, tz);
    int jd = month_start_jd + day - 1;
    lunar.year = year;
    lunar.month = month;
    lunar.leap = leap;
    lunar.month_size = next_month_start_jd - month_start_jd;
    lunar.day = day;
    lunar.jd = jd;
    lunar.valid = 1;
    return lunar;
}

struct ArgumentList lunarh_parse_argv(int argc, char **argv)
{
    struct ArgumentList argl = { 
        .task = LUNARH_GREGORIAN_TO_LUNAR_IN_CHINESE, .tz = 7.0, .valid = 0,
        .year = 0, .month = 0, .leap = 0, .day = 0,
        .show_month_size = 0
    };
    for (int i = 0; i < argc; i += 1)
    {
        if (strncmp("-y", argv[i], 3) == 0)
        {
            i += 1;
            if (i == argc) { return argl; }
            argl.year = strtol(argv[i], NULL, 10);
        }
        else if (strncmp("-m", argv[i], 3) == 0)
        {
            i += 1;
            if (i == argc) { return argl; }
            argl.month = strtol(argv[i], NULL, 10);
        }
        else if (strncmp("-d", argv[i], 3) == 0)
        {
            i += 1;
            if (i == argc) { return argl; }
            argl.day = strtol(argv[i], NULL, 10);
        }
        else if (strncmp("-tz", argv[i], 4) == 0)
        {
            i += 1;
            if (i == argc) { return argl; }
            argl.tz = strtod(argv[i], NULL);
        }
        else if (strncmp("-l", argv[i], 3) == 0)
        {
            argl.leap = 1;
        }
        else if (strncmp("-lm", argv[i], 4) == 0)
        {
            argl.task = LUNARH_GREGORIAN_TO_LUNAR_IN_MODERN_NOTATION;
        }
        else if (strncmp("-lms", argv[i], 5) == 0)
        {
            argl.show_month_size = 1;
        }
        else if (strncmp("-r", argv[i], 3) == 0)
        {
            argl.task = LUNARH_LUNAR_TO_GREGORIAN;
        }
        else if (strncmp("-ly", argv[i], 4) == 0)
        {
            argl.task = LUNARH_LUNAR_CALENDAR_OF_A_YEAR;
        }
        else if (strncmp("-", argv[i], 1) == 0)
        {
            return argl;
        }
    }
    if (
        argl.year == 0 && argl.month == 0 && argl.day == 0 && 
        (
            argl.task == LUNARH_GREGORIAN_TO_LUNAR_IN_CHINESE || 
            argl.task == LUNARH_GREGORIAN_TO_LUNAR_IN_MODERN_NOTATION ||
            argl.task == LUNARH_LUNAR_CALENDAR_OF_A_YEAR
        )
    )
    {
        time_t raw;
        time(&raw);
        struct tm* info = localtime(&raw);
        argl.year = info->tm_year + 1900;
        argl.month = info->tm_mon + 1;
        argl.day = info->tm_mday;
    }
    argl.valid = 1;
    return argl;
}

void lunarh_stringify_Chinese_numeral(int value, char *out, int out_size)
{
    if (value == 20)
    {
        sprintf_s(out, out_size, "二十");
    }
    else 
    {
        int tens = (int)floor(value / 10.0) * 10;
        int units = value - tens;
        sprintf_s(out, out_size, "%s%s", lunarh_get_Chinese_digit(tens), 
            lunarh_get_Chinese_digit(units));
    }
}

void lunarh_stringify_Lunar_date(struct Lunar date, char *out, int out_size, 
    int show_month_size)
{
    char year[64];
    lunarh_stringify_Lunar_year(date.year, year, 64);
    char month[64];
    lunarh_stringify_Lunar_month(date.month, date.leap, month, 64);
    char day[64];
    lunarh_stringify_Lunar_day(date.day, day, 64);
    sprintf_s(out, out_size, "%s%s%s%s", year, month, 
        show_month_size ? date.month_size == 29 ? "（小）" : "（大）" : "", day);
}

void lunarh_stringify_Lunar_day(int day, char *out, int out_size)
{
    char s[64];
    lunarh_stringify_Chinese_numeral(day, s, 64);
    sprintf_s(out, out_size, "%s%s日", day < 11 ? "初" : "", s);
}

void lunarh_stringify_Lunar_month(int month, int leap, char *out, int out_size)
{
    char s[64];
    lunarh_stringify_Chinese_numeral(month, s, 64);
    if (strncmp(s, "一", 64) == 0)
    {
        sprintf_s(s, 64, "正");
    }
    sprintf_s(out, out_size, "%s%s月", leap == 1 ? "閏" : "", s);
}

void lunarh_stringify_Lunar_year(int year, char *out, int out_size)
{
    sprintf_s(out, out_size, "%s%s年", lunarh_get_stem_of_year(year), 
        lunarh_get_branch_of_year(year));
}

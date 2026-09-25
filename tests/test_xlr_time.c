#include "xlr_calendar.h"
#include "xlr_logic.h"
#include "xlr_net_form.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool synced;
bool xlr_net_time_ready(void) { return synced; }

static time_t utc(int year, int month, int day, int hour, int minute) {
    struct tm t = {.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day,
                   .tm_hour = hour, .tm_min = minute};
    return timegm(&t);
}

int main(void) {
    xlr_moment_t m;
    assert(!xlr_calendar_now(&m));
    assert(!xlr_calendar_at(0, &m));
    assert(!xlr_calendar_at(4102444800LL, &m));
    assert(!xlr_calendar_at(utc(2026, 9, 25, 4, 0), NULL));
    /* HKO: https://www.hko.gov.hk/en/gts/time/calendar/pdf/files/2026e.pdf */
    assert(xlr_calendar_at(utc(2026, 9, 25, 4, 0), &m));
    assert(m.solar_year == 2026 && m.solar_month == 9 && m.solar_day == 25);
    assert(m.hour == 12 && m.lunar_month == 8 && m.lunar_day == 15 && !m.leap && m.shichen == 7);
    xlr_cast_t cast;
    assert(xlr_cast_numbers(m.lunar_month, m.lunar_day, m.shichen, &cast));
    assert(cast.path[0] == XLR_LIU_LIAN && cast.path[1] == XLR_CHI_KOU && cast.path[2] == XLR_CHI_KOU);
    /* 23:00 starts Zi, but this app's lunar date changes at 00:00. */
    assert(xlr_calendar_at(utc(2026, 9, 25, 14, 59), &m) && m.shichen == 12 && m.lunar_day == 15);
    assert(xlr_calendar_at(utc(2026, 9, 25, 15, 0), &m) && m.shichen == 1 && m.lunar_day == 15);
    assert(xlr_calendar_at(utc(2026, 9, 25, 16, 0), &m) && m.shichen == 1 && m.lunar_day == 16);
    assert(xlr_calendar_at(utc(2026, 9, 25, 17, 0), &m) && m.shichen == 2);
    for (int hour = 0; hour < 24; hour++) {
        assert(xlr_calendar_at(utc(2026, 9, 25, hour - 8, 0), &m));
        assert(m.hour == hour && m.shichen == (hour == 23 ? 1 : (hour + 1) / 2 + 1));
    }
    /* HKO 2025 almanac: July 25 = leap sixth month, day 1. */
    assert(xlr_calendar_at(utc(2025, 7, 25, 4, 0), &m));
    assert(m.lunar_month == 6 && m.lunar_day == 1 && m.leap);
    assert(xlr_calendar_at(utc(2026, 2, 17, 4, 0), &m));
    assert(m.lunar_month == 1 && m.lunar_day == 1 && !m.leap);

    char ssid[33], password[65], token[17];
    assert(xlr_net_parse_form("s=%E5%AE%B6%E4%B8%AD+Wi-Fi&p=a%26b%3Dc%2B12&t=0123456789abcdef", ssid, password, token));
    assert(!strcmp(ssid, "家中 Wi-Fi") && !strcmp(password, "a&b=c+12"));
    assert(xlr_net_parse_form("p=&t=0123456789abcdef&s=Open", ssid, password, token));
    assert(!password[0]);
    assert(xlr_net_parse_form("s=12345678901234567890123456789012&p=12345678&t=0123456789abcdef", ssid, password, token));
    const char *bad[] = {
        "s=&p=12345678&t=0123456789abcdef",
        "s=123456789012345678901234567890123&p=12345678&t=0123456789abcdef",
        "s=a&p=short&t=0123456789abcdef", "s=a&s=b&p=12345678&t=0123456789abcdef",
        "s=%00&p=12345678&t=0123456789abcdef", "s=%GG&p=12345678&t=0123456789abcdef",
        "s=%A&p=12345678&t=0123456789abcdef", "s=a%&p=12345678&t=0123456789abcdef",
        "s=a&p=12345678", "s=a&p=12345678&t=wrong", "s=a%0A&p=12345678&t=0123456789abcdef"
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        assert(!xlr_net_parse_form(bad[i], ssid, password, token));
    char form[180];
    snprintf(form, sizeof(form), "s=a&p=%064d&t=0123456789abcdef", 0);
    assert(xlr_net_parse_form(form, ssid, password, token));
    form[6] = 'g';
    assert(!xlr_net_parse_form(form, ssid, password, token));
    puts("xlr_time: calendar, boundaries, leap month and Wi-Fi form PASS");
}

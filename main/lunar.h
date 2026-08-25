enum Task
{
    LUNARH_GREGORIAN_TO_LUNAR_IN_CHINESE = 1,
    LUNARH_GREGORIAN_TO_LUNAR_IN_MODERN_NOTATION,
    LUNARH_LUNAR_TO_GREGORIAN,
    LUNARH_LUNAR_CALENDAR_OF_A_YEAR,
};

struct ArgumentList
{
    double tz;
    int valid;
    int year;
    int month;
    int leap;
    int day;
    int show_month_size;
    enum Task task;
};

struct Gregorian 
{
    double tz;
    int year;
    int month;
    int day;
    int jd;
    int valid;
};

struct Lunar 
{
    double tz;
    int year;
    int month;
    int month_size;
    int leap;
    int day;
    int jd;
    int valid;
};

struct Lunar lunarh_convert_Gregorian_to_Lunar(struct Gregorian date);
struct Gregorian lunarh_convert_Lunar_to_Gregorian(struct Lunar lunar);
void lunarh_display_Lunar_in_Chinese(struct ArgumentList argl);
void lunarh_display_Lunar_in_modern_notation(struct ArgumentList argl);
void lunarh_display_Gregorian(struct ArgumentList argl);
void lunarh_display_Lunar_calendar_of_a_year(struct ArgumentList argl);
const char* lunarh_get_branch_of_year(int year);
const char* lunarh_get_Chinese_digit(int value);
int lunarh_get_kth_new_moon_jd(int k, double tz);
int lunarh_get_leap_month_offset(int new_moon_11_jd, double tz);
int lunarh_get_lunar_month_11_start_jd(int year, double tz);
int lunarh_get_num_days_of_Gregorian_month(int year, int month);
const char* lunarh_get_stem_of_year(int year);
int lunarh_get_sun_longitude(int jd, double tz);
int lunarh_is_leap_Gregorian_year(int year);
int lunarh_is_valid_Gregorian(int year, int month, int day);
struct Gregorian lunarh_new_Gregorian(int year, int month, int day, double tz);
struct Gregorian lunarh_new_Gregorian_2(int jd, double tz);
struct Lunar lunarh_new_Lunar(int year, int month, int leap, int day, double tz);
struct ArgumentList lunarh_parse_argv(int argc, char** argv);
void lunarh_stringify_Chinese_numeral(int value, char* out, int out_size);
void lunarh_stringify_Lunar_date(struct Lunar date, char* out, int out_size, int show_month_size);
void lunarh_stringify_Lunar_day(int day, char* out, int out_size);
void lunarh_stringify_Lunar_month(int month, int leap, char* out, int out_size);
void lunarh_stringify_Lunar_year(int year, char* out, int out_size);

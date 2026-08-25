#include "xlr_calendar.h"
#include "xlr_net.h"
#include "lunar.h"
#include <time.h>
static const char *M[]={"","正月","二月","三月","四月","五月","六月","七月","八月","九月","十月","冬月","腊月"};
static const char *D[]={"","初一","初二","初三","初四","初五","初六","初七","初八","初九","初十","十一","十二","十三","十四","十五","十六","十七","十八","十九","二十","廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"};
static const char *S[]={"","子时","丑时","寅时","卯时","辰时","巳时","午时","未时","申时","酉时","戌时","亥时"};
bool xlr_calendar_now(xlr_moment_t *o){if(!o||!xlr_net_time_ready())return false;time_t t;time(&t);struct tm v;localtime_r(&t,&v);struct Gregorian g=lunarh_new_Gregorian(v.tm_year+1900,v.tm_mon+1,v.tm_mday,8);struct Lunar l=lunarh_convert_Gregorian_to_Lunar(g);if(!l.valid)return false;*o=(xlr_moment_t){v.tm_year+1900,v.tm_mon+1,v.tm_mday,v.tm_hour,v.tm_min,l.month,l.day,l.leap,v.tm_hour==23?1:((v.tm_hour+1)/2)+1};return true;}
const char *xlr_calendar_month_name(int n){return n>0&&n<13?M[n]:"未知";} const char *xlr_calendar_day_name(int n){return n>0&&n<31?D[n]:"未知";} const char *xlr_calendar_shichen_name(int n){return n>0&&n<13?S[n]:"未知";}

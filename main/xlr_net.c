#include "xlr_net.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "xlr_net";
static xlr_net_state_t s_state;
static char s_ap[24], s_ip[16] = "--";
static httpd_handle_t s_http;
static int s_retries;
static bool s_have_credentials;
static void wifi_credentials(wifi_config_t *c,const char *ssid,const char *pass){memset(c,0,sizeof(*c));memcpy(c->sta.ssid,ssid,strnlen(ssid,sizeof(c->sta.ssid)));memcpy(c->sta.password,pass,strnlen(pass,sizeof(c->sta.password)));}

static const char PAGE[] =
"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
"<style>body{font:16px system-ui;background:#f6f4ef;color:#171717;margin:0}.c{max-width:430px;margin:8vh auto;padding:24px}"
"form{background:#fff;border:1px solid #ddd9d2;border-radius:24px;padding:26px}h1{font-weight:500}label{display:block;margin:18px 0 6px}"
"input,button{box-sizing:border-box;width:100%;padding:14px;border-radius:13px;border:1px solid #ccc;font-size:16px}button{margin-top:22px;background:#222;color:white}</style>"
"<div class=c><h1>小六壬 · 连接网络</h1><p>填写家中 2.4GHz Wi‑Fi，设备将自动校准北京时间。</p>"
"<form method=post action=/save><label>Wi‑Fi 名称</label><input name=s maxlength=32 required>"
"<label>Wi‑Fi 密码</label><input name=p type=password maxlength=64><button>保存并连接</button></form></div>";

static void decode(char *s) {
    char *o = s;
    for (; *s; s++) { if (*s == '+') *o++ = ' '; else if (*s == '%' && s[1] && s[2]) { unsigned v; if (sscanf(s + 1, "%2x", &v) == 1) { *o++ = (char)v; s += 2; } } else *o++ = *s; }
    *o = 0;
}
static esp_err_t root(httpd_req_t *r) { httpd_resp_set_type(r,"text/html; charset=utf-8"); return httpd_resp_send(r,PAGE,HTTPD_RESP_USE_STRLEN); }
static esp_err_t save(httpd_req_t *r) {
    char body[512] = {0}, ssid[33] = {0}, pass[65] = {0};
    if (r->content_len <= 0 || r->content_len >= sizeof(body)) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"form too large");
    int total=0;
    while(total<r->content_len){int n=httpd_req_recv(r,body+total,r->content_len-total);if(n<=0)return ESP_FAIL;total+=n;}
    char *s = strstr(body,"s="), *p = strstr(body,"&p=");
    if (!s || !p) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"bad form");
    *p = 0; snprintf(ssid,sizeof(ssid),"%s",s+2); snprintf(pass,sizeof(pass),"%s",p+3); decode(ssid); decode(pass);
    nvs_handle_t h; if (nvs_open("wifi",NVS_READWRITE,&h)==ESP_OK) { nvs_set_str(h,"ssid",ssid); nvs_set_str(h,"pass",pass); nvs_commit(h); nvs_close(h); }
    httpd_resp_set_type(r,"text/html; charset=utf-8"); httpd_resp_sendstr(r,"<meta charset=utf-8><h2>已保存，设备正在连接……</h2><p>现在可以关闭此页面。</p>");
    wifi_config_t c; wifi_credentials(&c,ssid,pass);
    esp_wifi_set_config(WIFI_IF_STA,&c); s_state=XLR_NET_CONNECTING; s_retries=0; esp_wifi_connect(); return ESP_OK;
}
static void start_http(void) { httpd_config_t c=HTTPD_DEFAULT_CONFIG(); httpd_start(&s_http,&c); httpd_uri_t a={.uri="/",.method=HTTP_GET,.handler=root}; httpd_uri_t b={.uri="/save",.method=HTTP_POST,.handler=save}; httpd_register_uri_handler(s_http,&a); httpd_register_uri_handler(s_http,&b); }
static void sync_time(void) { setenv("TZ","CST-8",1); tzset(); esp_sntp_setoperatingmode(SNTP_OPMODE_POLL); esp_sntp_setservername(0,"ntp.aliyun.com"); esp_sntp_setservername(1,"pool.ntp.org"); esp_sntp_init(); }
static void event(void *a, esp_event_base_t base, int32_t id, void *data) {
    if (base==WIFI_EVENT && id==WIFI_EVENT_STA_START && s_have_credentials) { ESP_LOGI(TAG,"开始连接已保存的 Wi-Fi"); ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect()); }
    if (base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) { wifi_event_sta_disconnected_t *d=data; ESP_LOGW(TAG,"Wi-Fi 断开 reason=%d retry=%d",d?d->reason:-1,s_retries); if (++s_retries < 6) esp_wifi_connect(); else { s_state=XLR_NET_PROVISIONING; esp_wifi_set_mode(WIFI_MODE_APSTA); } }
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) { esp_netif_ip_info_t *i=&((ip_event_got_ip_t*)data)->ip_info; snprintf(s_ip,sizeof(s_ip),IPSTR,IP2STR(&i->ip)); s_state=XLR_NET_READY; s_retries=0; sync_time(); esp_wifi_set_mode(WIFI_MODE_STA); }
}
void xlr_net_start(void) {
    esp_err_t e=nvs_flash_init(); if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){nvs_flash_erase();nvs_flash_init();}
    esp_netif_init(); esp_event_loop_create_default(); esp_netif_create_default_wifi_sta(); esp_netif_create_default_wifi_ap();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&init); esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,NULL); esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,event,NULL);
    uint8_t mac[6]; esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP); snprintf(s_ap,sizeof(s_ap),"小六壬-%02X%02X",mac[4],mac[5]);
    wifi_config_t ap={.ap={.channel=1,.max_connection=4,.authmode=WIFI_AUTH_OPEN}}; snprintf((char*)ap.ap.ssid,sizeof(ap.ap.ssid),"%s",s_ap); ap.ap.ssid_len=strlen(s_ap);
    esp_wifi_set_mode(WIFI_MODE_APSTA); esp_wifi_set_config(WIFI_IF_AP,&ap);
    nvs_handle_t h; char ssid[33]={0},pass[65]={0}; size_t ns=sizeof(ssid),np=sizeof(pass); bool saved=false;
    if(nvs_open("wifi",NVS_READONLY,&h)==ESP_OK){saved=nvs_get_str(h,"ssid",ssid,&ns)==ESP_OK; nvs_get_str(h,"pass",pass,&np); nvs_close(h);}
    s_have_credentials=saved;
    if(saved){wifi_config_t c;wifi_credentials(&c,ssid,pass);esp_wifi_set_config(WIFI_IF_STA,&c);s_state=XLR_NET_CONNECTING;ESP_LOGI(TAG,"已载入 Wi-Fi：%s",ssid);}else s_state=XLR_NET_PROVISIONING;
    esp_wifi_start(); start_http(); ESP_LOGI(TAG,"配网热点 %s, http://192.168.4.1",s_ap);
}
xlr_net_state_t xlr_net_state(void){return s_state;} const char *xlr_net_ap_name(void){return s_ap;} const char *xlr_net_ip(void){return s_ip;}
bool xlr_net_time_ready(void){time_t now;time(&now);return now>1700000000;}

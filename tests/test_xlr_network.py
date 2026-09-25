#!/usr/bin/env python3
"""Exercise production network callbacks with host ESP-IDF fakes (no radio).
Run: python3 tests/test_xlr_network.py
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'main/xlr_net.c').read_text()
# Replace only platform headers; the production event/state code stays unchanged.
source = source.replace('#include "xlr_portal.h"\n', '')
source = re.sub(r'^#include "(?:esp_[^"]+|nvs[^"/]*|lwip/sockets|freertos/FreeRTOS)\.h"\n', '', source, flags=re.M)
stubs = r'''
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
typedef int esp_err_t;
typedef const char *esp_event_base_t;
#define CONFIG_LWIP_IPV6 1
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NO_MEM 2
#define ESP_EVENT_ANY_ID -1
#define ESP_EVENT_DEFINE_BASE(n) const char *n = #n
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
static const char *WIFI_EVENT = "wifi";
static const char *IP_EVENT = "ip";
enum { WIFI_MODE_STA, WIFI_MODE_APSTA, WIFI_IF_AP, WIFI_IF_STA, WIFI_STORAGE_RAM,
       WIFI_AUTH_WPA2_PSK, ESP_MAC_WIFI_SOFTAP, WIFI_EVENT_STA_START,
       WIFI_EVENT_STA_DISCONNECTED, IP_EVENT_STA_GOT_IP, WIFI_REASON_ASSOC_LEAVE,
       WIFI_EVENT_DPP_URI_READY, WIFI_EVENT_DPP_CFG_RECVD, WIFI_EVENT_DPP_FAILED, DPP_BOOTSTRAP_QR_CODE,
       NVS_READWRITE, NVS_READONLY, HTTP_GET, HTTP_POST };
typedef int httpd_err_code_t;
#define HTTPD_404_NOT_FOUND 404
#define HTTPD_403_FORBIDDEN "403"
#define HTTPD_400_BAD_REQUEST "400"
#define HTTPD_408_REQ_TIMEOUT "408"
#define HTTPD_500_INTERNAL_SERVER_ERROR "500"
typedef struct { unsigned char ssid[32], password[64]; } station_t;
typedef struct { station_t sta; struct { unsigned char ssid[32], password[64]; int channel, max_connection, authmode, ssid_len; } ap; } wifi_config_t;
typedef struct { int reason; } wifi_event_sta_disconnected_t;
typedef struct { uint32_t uri_data_len; char uri[]; } wifi_event_dpp_uri_ready_t;
typedef struct { wifi_config_t wifi_cfg; } wifi_event_dpp_config_received_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
static int dpp_inits, dpp_deinits, dpp_listens;
static int esp_supp_dpp_init(void *cb) { dpp_inits++; return 0; }
static int esp_supp_dpp_deinit(void) { dpp_deinits++; return 0; }
static int esp_supp_dpp_start_listen(void) { dpp_listens++; return 0; }
static int esp_supp_dpp_stop_listen(void) { return 0; }
static int esp_supp_dpp_bootstrap_gen(const char *channels,int type,const char *key,const char *info) {
    assert(!strcmp(channels,"6") && key == NULL); return 0;
}
typedef int wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() 0
typedef int esp_netif_t;
typedef struct { struct { uint32_t addr; } ip; } esp_netif_ip_info_t;
typedef struct { bool start, wait_for_sync; void (*sync_cb)(struct timeval *); } esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(...) ((esp_sntp_config_t){0})
typedef void *httpd_handle_t;
typedef struct { size_t content_len; } httpd_req_t;
typedef struct { int max_open_sockets, lru_purge_enable, recv_wait_timeout; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
typedef struct { const char *uri; int method; esp_err_t (*handler)(httpd_req_t *); } httpd_uri_t;
typedef int nvs_handle_t;
static int clock_s, connects, sync_starts, mode, posts, posted_id, close_count;
static int nvs_error, wifi_error;
static struct sockaddr_storage local_address;
static socklen_t local_length;
static bool socket_ok;
static const char *request_body;
static char response_body[12288];
static int fake_getsockname(int fd, struct sockaddr *address, socklen_t *length) {
    if (!socket_ok) return -1;
    if (*length > local_length) *length = local_length;
    memcpy(address, &local_address, *length);
    return 0;
}
#define getsockname fake_getsockname
static void local_ip(const char *ip, int family) {
    memset(&local_address, 0, sizeof(local_address));
    local_address.ss_family = family;
    if (family == AF_INET) {
        local_length = sizeof(struct sockaddr_in);
        assert(inet_pton(family, ip, &((struct sockaddr_in *)&local_address)->sin_addr) == 1);
    } else {
        local_length = sizeof(struct sockaddr_in6);
        assert(inet_pton(family, ip, &((struct sockaddr_in6 *)&local_address)->sin6_addr) == 1);
    }
    socket_ok = true;
}
static int portal_starts, portal_stops, portal_error;
static int xlr_portal_start(esp_netif_t *netif) { portal_starts++; return portal_error; }
static void xlr_portal_stop(void) { portal_stops++; }
static esp_netif_t fake_netif;
static wifi_config_t saved_config;
static bool saved;
static void (*on_sync)(struct timeval *);
static int64_t esp_timer_get_time(void) { return (int64_t)clock_s * 1000000; }
static int esp_event_post(esp_event_base_t b, int id, const void *d, size_t n, int wait) { posts++; posted_id=id; return 0; }
static int esp_event_handler_register(esp_event_base_t b, int id, void (*f)(void*,esp_event_base_t,int32_t,void*), void *a) { return 0; }
static int esp_event_loop_create_default(void) { return 0; }
static int esp_netif_init(void) { return 0; }
static esp_netif_t *esp_netif_create_default_wifi_sta(void) { return &fake_netif; }
static esp_netif_t *esp_netif_create_default_wifi_ap(void) { return &fake_netif; }
static int esp_netif_get_ip_info(esp_netif_t *n, esp_netif_ip_info_t *i) { i->ip.addr=inet_addr("192.168.4.1"); return 0; }
static int esp_wifi_init(wifi_init_config_t *c) { return 0; }
static int esp_wifi_set_storage(int s) { return 0; }
static int esp_wifi_set_mode(int m) { mode=m; return 0; }
static int esp_wifi_set_config(int i, wifi_config_t *c) { return wifi_error; }
typedef struct { uint8_t ssid[33]; } wifi_ap_record_t;
static wifi_ap_record_t scan_results[20];
static int scan_count, scan_error, scan_reads_error, scan_clears;
static int esp_wifi_scan_start(void *config, bool block) { return scan_error; }
static int esp_wifi_scan_get_ap_records(uint16_t *n, wifi_ap_record_t *out) {
    if (scan_reads_error) return scan_reads_error;
    if (*n > scan_count) *n = scan_count;
    memcpy(out, scan_results, *n * sizeof(*out)); return 0;
}
static int esp_wifi_clear_ap_list(void) { scan_clears++; return 0; }
static int esp_wifi_start(void) { return 0; }
static int esp_wifi_connect(void) { connects++; return wifi_error; }
static int esp_wifi_disconnect(void) { return 0; }
static int esp_read_mac(uint8_t *m,int k) { memset(m, 1, 6); return 0; }
static uint32_t esp_random(void) { return 0x0123abcd; }
static int esp_netif_sntp_init(const esp_sntp_config_t *c) { on_sync=c->sync_cb; return 0; }
static int esp_netif_sntp_start(void) { sync_starts++; return 0; }
static int nvs_flash_init(void) { return 0; }
static int nvs_open(const char *n, int m, nvs_handle_t *h) { *h=1; return nvs_error; }
static int nvs_set_blob(nvs_handle_t h,const char *k,const void *d,size_t n) { memcpy(&saved_config,d,n); saved=true; return nvs_error; }
static int nvs_get_blob(nvs_handle_t h,const char *k,void *d,size_t *n) { if (!saved) return 1; memcpy(d,&saved_config,*n); return 0; }
static int nvs_commit(nvs_handle_t h) { return nvs_error; }
static void nvs_close(nvs_handle_t h) {}
static int httpd_start(httpd_handle_t *h,const httpd_config_t *c) { *h=(void*)1; return 0; }
static int httpd_stop(httpd_handle_t h) { close_count++; return 0; }
static int httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t *u) { return 0; }
static char redirect_location[64], response_status[32];
static int httpd_register_err_handler(httpd_handle_t h, int code, esp_err_t (*handler)(httpd_req_t *,httpd_err_code_t)) { assert(code == 404); return 0; }
static int httpd_resp_set_status(httpd_req_t *r,const char *status) { snprintf(response_status,sizeof(response_status),"%s",status); return 0; }
static int httpd_resp_send_err(httpd_req_t *r,const char *s,const char *b) { return -1; }
static int httpd_resp_set_type(httpd_req_t *r,const char *s) { return 0; }
static int httpd_resp_set_hdr(httpd_req_t *r,const char *n,const char *v) { if (!strcmp(n,"Location")) snprintf(redirect_location,sizeof(redirect_location),"%s",v); return 0; }
static int httpd_resp_sendstr(httpd_req_t *r,const char *s) { snprintf(response_body,sizeof(response_body),"%s",s); return 0; }
static int httpd_req_recv(httpd_req_t *r,char *s,size_t n) { if (!request_body) return -1; memcpy(s, request_body, n); request_body += n; return (int)n; }
static int httpd_req_to_sockfd(httpd_req_t *r) { return -1; }
'''
checks = r'''
int main(void) {
    xlr_net_start();
    assert(s_initialized && on_sync && !s_ap_active && !xlr_net_time_ready());
    httpd_req_t request = {0};
    char escaped[193];
    escape_ssid((const uint8_t *)"a<&\"'", escaped);
    assert(!strcmp(escaped, "a&lt;&amp;&quot;&#39;"));

    assert(root(&request) < 0 && save(&request) < 0); /* not on the setup AP */
    xlr_net_start(); /* idempotent */
    assert(!connects);
    xlr_net_provision();
    assert(posted_id == REQUEST_PROVISION);
    event(NULL, XLR_NET_EVENT, posted_id, NULL);
    local_ip("192.168.4.1", AF_INET);
    assert(portal_starts == 1);
    assert(portal_redirect(&request, 404) == 0);
    assert(!strcmp(redirect_location, "http://192.168.4.1/"));
    assert(!strcmp(response_status, "302 Found") && strlen(response_body));
    local_ip("192.168.1.2", AF_INET);
    assert(portal_redirect(&request, 404) < 0);
    local_ip("192.168.4.1", AF_INET);
    scan_count = 3;
    strcpy((char *)scan_results[0].ssid, "家里<&\"");
    scan_results[1] = scan_results[0];
    strcpy((char *)scan_results[2].ssid, "Other");
    assert(root(&request) == 0);
    const char *option = "<option value=\"家里&lt;&amp;&quot;\">";
    char *found = strstr(response_body, option);
    assert(found && !strstr(found + strlen(option), option));
    assert(strstr(response_body, "<option value=\"Other\">"));
    assert(!strstr(response_body, "type=checkbox checked"));
    scan_count = 20;
    for (int i = 0; i < 20; i++) {
        memset(scan_results[i].ssid, '"', 32);
        scan_results[i].ssid[0] = 'A' + i;
        scan_results[i].ssid[32] = 0;
    }
    assert(root(&request) == 0 && strstr(response_body, "</script></html>"));
    assert(strlen(response_body) < sizeof(response_body) - 1);
    scan_error = 1;
    assert(root(&request) == 0 && strstr(response_body, "type=checkbox checked"));
    scan_error = 0; scan_reads_error = 1;
    assert(root(&request) == 0 && scan_clears == 1);
    scan_reads_error = 0; scan_count = 0;

    assert(s_ap_active && mode == WIFI_MODE_APSTA && s_http);
    const char *allowed[] = {"192.168.4.1", "::ffff:192.168.4.1"};
    for (int i=0; i<2; i++) {
        local_ip(allowed[i], i ? AF_INET6 : AF_INET);
        assert(root(&request) == 0 && strstr(response_body, "<form method=post"));
        assert(strstr(response_body, "</html>"));
        char form[128];
        snprintf(form, sizeof(form), "s=test&p=12345678&t=%s", s_token);
        request_body = form;
        request.content_len = strlen(form);
        assert(save(&request) == 0 && posted_id == REQUEST_CONNECT);
    }
    local_ip("192.168.1.50", AF_INET);
    assert(root(&request) < 0 && save(&request) < 0);
    local_ip("::ffff:192.168.1.50", AF_INET6);
    assert(root(&request) < 0 && save(&request) < 0);
    local_ip("::1", AF_INET6);
    assert(root(&request) < 0 && save(&request) < 0);
    local_ip("::ffff:192.168.4.1", AF_INET6);
    local_length=sizeof(struct sockaddr_in); /* Reject a truncated IPv6 address. */
    assert(root(&request) < 0);
    socket_ok=false;
    assert(root(&request) < 0);

    wifi_config_t config = {0};
    memcpy(config.sta.ssid, "test", 4);
    nvs_error=1;
    event(NULL, XLR_NET_EVENT, REQUEST_CONNECT, &config);
    assert(xlr_net_state() == XLR_NET_FAILED && !connects);
    nvs_error=0;
    event(NULL, XLR_NET_EVENT, REQUEST_CONNECT, &config);
    assert(saved && connects == 1 && xlr_net_state() == XLR_NET_CONNECTING);
    clock_s=61;
    assert(xlr_net_state() == XLR_NET_FAILED && !xlr_net_time_ready());
    event(NULL, XLR_NET_EVENT, REQUEST_RETRY, NULL);
    assert(connects == 2 && xlr_net_state() == XLR_NET_CONNECTING);
    wifi_event_sta_disconnected_t disconnected = {.reason=1};
    for (int i=0; i<6; i++) event(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnected);
    assert(connects == 7 && xlr_net_state() == XLR_NET_FAILED);
    event(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    assert(sync_starts == 1 && xlr_net_state() == XLR_NET_SYNCING && !xlr_net_time_ready());
    clock_s += 61;
    assert(xlr_net_state() == XLR_NET_FAILED);
    event(NULL, XLR_NET_EVENT, REQUEST_RETRY, NULL);
    assert(sync_starts == 2 && xlr_net_state() == XLR_NET_SYNCING);
    struct timeval t={.tv_sec=0};
    on_sync(&t);
    assert(!xlr_net_time_ready());
    t.tv_sec=1790294400;
    on_sync(&t);
    assert(xlr_net_time_ready() && posted_id == TIME_SYNCED);
    event(NULL, XLR_NET_EVENT, posted_id, NULL);
    assert(xlr_net_state() == XLR_NET_READY && !s_ap_active && !s_http && close_count == 1);
    clock_s += 1000;
    event(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnected);
    assert(xlr_net_state() == XLR_NET_CONNECTING); /* a fresh reconnect gets its own timeout */
    assert(xlr_net_time_ready()); /* brief offline operation remains available */
    clock_s += 86400;
    assert(!xlr_net_time_ready());
    on_sync(&t);
    assert(xlr_net_time_ready());
    event(NULL, XLR_NET_EVENT, REQUEST_PROVISION, NULL);
    event(NULL, XLR_NET_EVENT, CLOSE_PROVISION, NULL);
    assert(!s_ap_active && !s_http && close_count == 2);
    s_synced_at=-1; /* cold boot must require a new successful SNTP callback */
    assert(!xlr_net_time_ready());
    s_initialized=false;
    s_have_credentials=false;
    connects=0;
    xlr_net_start();
    event(NULL, WIFI_EVENT, WIFI_EVENT_STA_START, NULL);
    assert(s_have_credentials && connects == 1 && !xlr_net_time_ready());
    xlr_net_easy_connect();
    assert(posted_id == REQUEST_DPP);
    event(NULL, XLR_NET_EVENT, posted_id, NULL);
    assert(xlr_net_dpp_active() && dpp_inits == 1 && !xlr_net_ap_active());
    char uri_copy[256];
    assert(!xlr_net_dpp_uri(uri_copy, sizeof(uri_copy)));
    struct { uint32_t uri_data_len; char uri[256]; } uri = {0};
    strcpy(uri.uri, "DPP:C:81/6;K:test;;");
    uri.uri_data_len = strlen(uri.uri) + 1;
    event(NULL, WIFI_EVENT, WIFI_EVENT_DPP_URI_READY, &uri);
    assert(xlr_net_dpp_uri(uri_copy, sizeof(uri_copy)) && !strcmp(uri_copy, uri.uri));
    assert(dpp_listens == 1 && !xlr_net_dpp_uri(uri_copy, 2));
    event(NULL, XLR_NET_EVENT, TIME_SYNCED, NULL);
    assert(xlr_net_dpp_active() && xlr_net_state() == XLR_NET_DPP);
    int before_connects=connects;
    event(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnected);
    assert(connects == before_connects);
    clock_s += 120;
    assert(xlr_net_state() == XLR_NET_FAILED);
    event(NULL, XLR_NET_EVENT, REQUEST_RETRY, NULL);
    assert(dpp_inits == 2 && dpp_deinits == 1);
    assert(!xlr_net_dpp_uri(uri_copy, sizeof(uri_copy)));
    event(NULL, WIFI_EVENT, WIFI_EVENT_DPP_URI_READY, &uri);
    wifi_event_dpp_config_received_t received={0};
    strcpy((char*)received.wifi_cfg.sta.ssid, "Home24");
    strcpy((char*)received.wifi_cfg.sta.password, "testpass");
    event(NULL, WIFI_EVENT, WIFI_EVENT_DPP_CFG_RECVD, &received);
    assert(!xlr_net_dpp_active() && xlr_net_state() == XLR_NET_CONNECTING);
    assert(!strcmp((char*)saved_config.sta.ssid, "Home24"));
    event(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    on_sync(&t);
    event(NULL, XLR_NET_EVENT, TIME_SYNCED, NULL);
    assert(dpp_deinits == 2 && xlr_net_time_ready());
    before_connects=connects;
    event(NULL, WIFI_EVENT, WIFI_EVENT_DPP_CFG_RECVD, &received);
    assert(connects == before_connects); /* ignore unsolicited credentials */
    event(NULL, XLR_NET_EVENT, REQUEST_DPP, NULL);
    event(NULL, XLR_NET_EVENT, REQUEST_PROVISION, NULL);
    assert(!xlr_net_dpp_active() && xlr_net_ap_active());
    event(NULL, XLR_NET_EVENT, CLOSE_PROVISION, NULL);
    assert(!xlr_net_ap_active());
    close_provision();
    int stopped = portal_stops;
    assert(provision() == ESP_OK);
    assert(close_provision() == ESP_OK && portal_stops == stopped + 1);
    portal_error = 1;
    assert(provision() != ESP_OK && !s_http && !s_ap_active && mode == WIFI_MODE_STA);
    puts("xlr_network: AP access, provisioning, NTP, Easy Connect and fallback PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='xlr-net-test-') as tmp:
    c = Path(tmp) / 'network.c'
    c.write_text(stubs + '\n' + source + '\n' + checks)
    binary = Path(tmp) / 'network'
    subprocess.run(['cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-I' + str(root / 'main'),
                    str(c), str(root / 'main/xlr_net_form.c'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

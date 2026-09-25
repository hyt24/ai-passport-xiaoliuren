#include "xlr_net.h"
#include "xlr_portal.h"
#include "xlr_net_form.h"
#include "esp_event.h"
#include "esp_dpp.h"
#include "freertos/FreeRTOS.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

ESP_EVENT_DEFINE_BASE(XLR_NET_EVENT);
enum { REQUEST_PROVISION, REQUEST_CONNECT, REQUEST_RETRY, TIME_SYNCED, CLOSE_PROVISION, REQUEST_DPP };
static const char *TAG = "xlr_net";
static atomic_int s_state, s_attempt_at, s_synced_at = -1;
static atomic_bool s_ap_active, s_online;
static bool s_initialized, s_have_credentials;
static int s_retries;
static char s_ap[24], s_password[9], s_token[17];
static httpd_handle_t s_http;
static esp_netif_t *s_ap_netif;
static atomic_bool s_dpp_active;
static bool s_dpp_initialized;
static char s_dpp_uri[256];
static portMUX_TYPE s_dpp_lock = portMUX_INITIALIZER_UNLOCKED;

static void stop_dpp(void) {
    s_dpp_active = false;
    if (s_dpp_initialized) {
        esp_supp_dpp_stop_listen();
        esp_supp_dpp_deinit();
        s_dpp_initialized = false;
    }
    portENTER_CRITICAL(&s_dpp_lock);
    s_dpp_uri[0] = 0;
    portEXIT_CRITICAL(&s_dpp_lock);
}

static int uptime(void) { return (int)(esp_timer_get_time() / 1000000); }
static void set_state(xlr_net_state_t state) { s_attempt_at = uptime(); s_state = state; }

static bool local_request(httpd_req_t *r) {
    struct sockaddr_storage addr = {0};
    socklen_t len = sizeof(addr);
    esp_netif_ip_info_t info;
    if (!s_ap_active || esp_netif_get_ip_info(s_ap_netif, &info) != ESP_OK ||
        getsockname(httpd_req_to_sockfd(r), (struct sockaddr *)&addr, &len) != 0)
        return false;
    if (addr.ss_family == AF_INET && len >= sizeof(struct sockaddr_in))
        return ((struct sockaddr_in *)&addr)->sin_addr.s_addr == info.ip.addr;
#if CONFIG_LWIP_IPV6
    /* ESP-IDF's dual-stack HTTP listener reports IPv4 as ::ffff:a.b.c.d. */
    if (addr.ss_family == AF_INET6 && len >= sizeof(struct sockaddr_in6)) {
        const struct in6_addr *ip = &((struct sockaddr_in6 *)&addr)->sin6_addr;
        return IN6_IS_ADDR_V4MAPPED(ip) &&
               memcmp(&ip->s6_addr[12], &info.ip.addr, sizeof(info.ip.addr)) == 0;
    }
#endif
    return false;
}

/* SSIDs are untrusted radio data, including when used as option values. */
static void escape_ssid(const uint8_t *ssid, char out[193]) {
    char *p = out;
    for (int i = 0; i < 32 && ssid[i]; i++) {
        const char *entity = NULL;
        switch (ssid[i]) {
        case '&': entity = "&amp;"; break;
        case '<': entity = "&lt;"; break;
        case '>': entity = "&gt;"; break;
        case '"': entity = "&quot;"; break;
        case 39: entity = "&#39;"; break;
        }
        if (entity) { size_t n = strlen(entity); memcpy(p, entity, n); p += n; }
        else *p++ = ssid[i];
    }
    *p = 0;
}

static esp_err_t root(httpd_req_t *r) {
    if (!local_request(r)) return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Use device setup Wi-Fi");
    /* Bounded heap buffers keep scanning and HTML off the small HTTP task stack. */
    char *page = malloc(12288);
    wifi_ap_record_t *aps = calloc(20, sizeof(*aps));
    if (!page || !aps) {
        free(page); free(aps);
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Please retry");
    }
    uint16_t count = 20;
    bool scanned = false;
    if (s_state != XLR_NET_CONNECTING && esp_wifi_scan_start(NULL, true) == ESP_OK) {
        scanned = esp_wifi_scan_get_ap_records(&count, aps) == ESP_OK;
        if (!scanned) esp_wifi_clear_ap_list();
    }
    if (!scanned) count = 0;
    size_t used = snprintf(page, 12288,
        "<!doctype html><html lang=zh-CN><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>小六壬配网</title><style>body{font:18px system-ui;max-width:420px;"
        "margin:32px auto;padding:20px;background:#e9e1cf;color:#28231d}"
        "label{display:block;margin-top:20px}input,select,button{box-sizing:border-box;"
        "width:100%%;font:inherit;padding:12px}input[type=checkbox]{width:auto}"
        "button{margin-top:24px}[hidden]{display:none}</style>"
        "<h1>连接网络，校准时间</h1><p>选择家里的 2.4GHz Wi-Fi，再填写密码。</p>"
        "<form method=post action=/save><input type=hidden name=t value='%s'>"
        "<label for=net>附近的 Wi-Fi</label><select id=net name=s required>"
        "<option value=''>请选择 Wi-Fi</option>", s_token);
    int visible = 0;
    for (int i = 0; i < count; i++) {
        aps[i].ssid[32] = 0;
        if (!aps[i].ssid[0] || !strcmp((char *)aps[i].ssid, s_ap)) continue;
        bool skip = false;
        for (int j = 0; j < i; j++)
            if (!strcmp((char *)aps[i].ssid, (char *)aps[j].ssid)) skip = true;
        for (int j = 0; j < 32 && aps[i].ssid[j]; j++)
            if (aps[i].ssid[j] < 32 || aps[i].ssid[j] == 127) skip = true;
        if (skip) continue;
        char escaped[193];
        escape_ssid(aps[i].ssid, escaped);
        used += snprintf(page + used, 12288 - used, "<option value=\"%s\">%s</option>", escaped, escaped);
        visible++;
    }
    free(aps);
    snprintf(page + used, 12288 - used,
        "</select><p>%s <a href='/'>重新扫描</a></p>"
        "<label><input id=manual type=checkbox%s>手动填写 / 隐藏网络</label>"
        "<label id=manualLabel for=s>Wi-Fi 名称</label><input id=s name=s autocomplete=off>"
        "<label for=p>Wi-Fi 密码</label><input id=p name=p type=password maxlength=64 "
        "autocomplete=new-password><p>开放网络请留空。</p><button>保存并连接</button></form>"
        "<p>保存后查看设备屏幕，等待校时成功。</p>"
        "<script nonce='%s'>const m=document.getElementById('manual'),"
        "n=document.getElementById('net'),s=document.getElementById('s'),"
        "l=document.getElementById('manualLabel');"
        "function toggle(){n.disabled=m.checked;n.required=!m.checked;"
        "s.disabled=!m.checked;s.required=m.checked;s.hidden=!m.checked;l.hidden=!m.checked}"
        "m.addEventListener('change',toggle);toggle();</script></html>",
        visible ? "未找到想连接的网络？" : "暂未扫描到网络，可重新扫描或手动填写。",
        visible ? "" : " checked", s_token);
    char policy[200];
    snprintf(policy, sizeof(policy), "default-src 'none'; style-src 'unsafe-inline'; script-src 'nonce-%s'; form-action 'self'; frame-ancestors 'none'", s_token);
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "Content-Security-Policy", policy);
    esp_err_t err = httpd_resp_sendstr(r, page);
    free(page);
    return err;
}

static esp_err_t save(httpd_req_t *r) {
    if (!local_request(r)) return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Use device setup Wi-Fi");
    char body[512] = {0}, ssid[33], password[65], token[17];
    if (r->content_len <= 0 || r->content_len >= sizeof(body))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Form too large");
    int total = 0;
    while (total < r->content_len) {
        int n = httpd_req_recv(r, body + total, r->content_len - total);
        if (n <= 0) return httpd_resp_send_err(r, HTTPD_408_REQ_TIMEOUT, "Please retry");
        total += n;
    }
    if (memchr(body, 0, total) || !xlr_net_parse_form(body, ssid, password, token))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid SSID or password (8-63 bytes, 64 hex, or empty)");
    if (strcmp(token, s_token)) return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Reload setup page");
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, strlen(ssid));
    memcpy(config.sta.password, password, strlen(password));
    if (esp_event_post(XLR_NET_EVENT, REQUEST_CONNECT, &config, sizeof(config), 0) != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Device busy; retry");
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, "<meta charset=utf-8><h2>正在连接</h2><p>请查看设备屏幕。校时成功后配网热点自动关闭；失败时可在此重新填写。</p>");
}

/* Connectivity probes must receive a redirect with a body (including on iOS). */
static esp_err_t portal_redirect(httpd_req_t *r, httpd_err_code_t error) {
    (void)error;
    if (!local_request(r)) return httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Use device setup Wi-Fi");
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, "<a href='http://192.168.4.1/'>Open Wi-Fi setup</a>");
}

static esp_err_t provision(void) {
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) return err;
    if (!s_http) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_open_sockets = 2;
        config.lru_purge_enable = true;
        config.recv_wait_timeout = 5;
        err = httpd_start(&s_http, &config);
        if (err == ESP_OK) {
            httpd_uri_t get = {.uri = "/", .method = HTTP_GET, .handler = root};
            httpd_uri_t post = {.uri = "/save", .method = HTTP_POST, .handler = save};
            err = httpd_register_uri_handler(s_http, &get);
            if (err == ESP_OK) err = httpd_register_uri_handler(s_http, &post);
            if (err == ESP_OK) err = httpd_register_err_handler(s_http, HTTPD_404_NOT_FOUND, portal_redirect);
        }
        if (err != ESP_OK) {
            if (s_http) { httpd_stop(s_http); s_http = NULL; }
            esp_wifi_set_mode(WIFI_MODE_STA);
            return err;
        }
    }
    err = xlr_portal_start(s_ap_netif);
    if (err != ESP_OK) {
        if (s_http) { httpd_stop(s_http); s_http = NULL; }
        esp_wifi_set_mode(WIFI_MODE_STA);
        return err;
    }
    s_ap_active = true;
    set_state(XLR_NET_PROVISIONING);
    return ESP_OK;
}

static esp_err_t close_provision(void) {
    if (!s_ap_active) return ESP_OK;
    s_ap_active = false;
    xlr_portal_stop();
    if (s_http) { httpd_stop(s_http); s_http = NULL; }
    return esp_wifi_set_mode(WIFI_MODE_STA);
}

static void sync_callback(struct timeval *tv) {
    /* A plausible clock alone is not proof of a sync during this boot. */
    if (tv && tv->tv_sec >= 1704067200LL && tv->tv_sec < 4102444800LL) {
        s_synced_at = uptime();
        esp_event_post(XLR_NET_EVENT, TIME_SYNCED, NULL, 0, 0);
    }
}

static esp_err_t connect_station(void) {
    s_retries = 0;
    set_state(XLR_NET_CONNECTING);
    return esp_wifi_connect();
}

static esp_err_t save_and_connect(wifi_config_t *config) {
    esp_err_t err;
    /* RAM Wi-Fi config; one NVS blob avoids partial SSID/password updates. */
    nvs_handle_t h;
    err = nvs_open("xlr_wifi", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, "config", config, sizeof(wifi_config_t));
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (err == ESP_OK) {
        esp_wifi_disconnect();
        s_online = false;
        err = esp_wifi_set_config(WIFI_IF_STA, config);
        if (err == ESP_OK) { s_have_credentials = true; err = connect_station(); }
    }
    return err;
}

static esp_err_t begin_dpp(void) {
    stop_dpp();
    esp_err_t err = close_provision();
    if (err != ESP_OK) return err;
    s_dpp_active = true;
    esp_wifi_disconnect();
    s_online = false;
    set_state(XLR_NET_DPP);
    err = esp_supp_dpp_init(NULL);
    if (err != ESP_OK) { s_dpp_active = false; return err; }
    s_dpp_initialized = true;
    err = esp_supp_dpp_bootstrap_gen("6", DPP_BOOTSTRAP_QR_CODE, NULL, "XiaoLiuRen");
    if (err != ESP_OK) stop_dpp();
    return err;
}

static void event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    esp_err_t err = ESP_OK;
    if (base == XLR_NET_EVENT) {
        if (id == REQUEST_PROVISION) { stop_dpp(); err = provision(); }
        else if (id == REQUEST_DPP) err = begin_dpp();
        else if (id == REQUEST_CONNECT) {
            err = save_and_connect(data);
        } else if (id == REQUEST_RETRY) {
            if (s_dpp_active) err = begin_dpp();
            else if (s_online) { set_state(XLR_NET_SYNCING); err = esp_netif_sntp_start(); }
            else if (s_have_credentials) err = connect_station();
            else err = provision();
        } else if (id == TIME_SYNCED) {
            if (s_dpp_active) return; /* A late sync from the old network must not cancel setup. */
            stop_dpp();
            set_state(XLR_NET_READY);
            err = close_provision();
        } else if (id == CLOSE_PROVISION) {
            stop_dpp();
            err = close_provision();
            if (s_state == XLR_NET_PROVISIONING || s_state == XLR_NET_DPP)
                set_state(xlr_net_time_ready() ? XLR_NET_READY : XLR_NET_OFFLINE);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_DPP_URI_READY && s_dpp_active) {
        wifi_event_dpp_uri_ready_t *uri = data;
        if (!uri || uri->uri_data_len < 6 || uri->uri_data_len > sizeof(s_dpp_uri) ||
            uri->uri[uri->uri_data_len - 1] != 0 || strncmp(uri->uri, "DPP:", 4)) {
            set_state(XLR_NET_FAILED);
            return;
        }
        portENTER_CRITICAL(&s_dpp_lock);
        memcpy(s_dpp_uri, uri->uri, uri->uri_data_len);
        portEXIT_CRITICAL(&s_dpp_lock);
        err = esp_supp_dpp_start_listen();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_DPP_CFG_RECVD && s_dpp_active) {
        wifi_event_dpp_config_received_t *received = data;
        /* Persist ordinary home-network credentials, not transient DPP-AKM connector state. */
        if (!received || !received->wifi_cfg.sta.ssid[0] || !received->wifi_cfg.sta.password[0]) {
            set_state(XLR_NET_FAILED);
            return;
        }
        esp_supp_dpp_stop_listen();
        err = save_and_connect(&received->wifi_cfg);
        if (err == ESP_OK) s_dpp_active = false;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_DPP_FAILED && s_dpp_active) {
        set_state(XLR_NET_FAILED);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_have_credentials) err = connect_station();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        /* The explicit disconnect when replacing credentials is already followed by connect(). */
        if (s_dpp_active || (d && d->reason == WIFI_REASON_ASSOC_LEAVE)) return;
        s_online = false;
        if (s_have_credentials && ++s_retries <= 5) {
            if (s_state != XLR_NET_CONNECTING) set_state(XLR_NET_CONNECTING);
            err = esp_wifi_connect();
        } else set_state(XLR_NET_FAILED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        if (s_dpp_active) return;
        s_online = true;
        s_retries = 0;
        set_state(XLR_NET_SYNCING);
        err = esp_netif_sntp_start();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Network operation failed: %s", esp_err_to_name(err));
        set_state(XLR_NET_FAILED);
    }
}

void xlr_net_start(void) {
    if (s_initialized) return;
    /* Do not erase unrelated NVS data if initialization fails. */
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) goto fail;
    err = esp_netif_init();
    if (err != ESP_OK) goto fail;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;
    if (!esp_netif_create_default_wifi_sta() || !(s_ap_netif = esp_netif_create_default_wifi_ap())) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail;
    uint8_t mac[6];
    err = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (err != ESP_OK) goto fail;
    snprintf(s_ap, sizeof(s_ap), "XiaoLiuRen-%02X%02X", mac[4], mac[5]);
    snprintf(s_password, sizeof(s_password), "%08lX", (unsigned long)esp_random());
    snprintf(s_token, sizeof(s_token), "%08lX%08lX", (unsigned long)esp_random(), (unsigned long)esp_random());
    wifi_config_t ap = {.ap = {.channel = 1, .max_connection = 2, .authmode = WIFI_AUTH_WPA2_PSK}};
    memcpy(ap.ap.ssid, s_ap, strlen(s_ap));
    ap.ap.ssid_len = strlen(s_ap);
    memcpy(ap.ap.password, s_password, strlen(s_password));
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) goto fail;
    nvs_handle_t h;
    wifi_config_t station = {0};
    if (nvs_open("xlr_wifi", NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(station);
        s_have_credentials = nvs_get_blob(h, "config", &station, &len) == ESP_OK && len == sizeof(station) && station.sta.ssid[0];
        nvs_close(h);
    }
    if (s_have_credentials && (err = esp_wifi_set_config(WIFI_IF_STA, &station)) != ESP_OK) goto fail;
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
        ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "pool.ntp.org"));
    sntp.start = false;
    sntp.wait_for_sync = false;
    sntp.sync_cb = sync_callback;
    err = esp_netif_sntp_init(&sntp);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_register(XLR_NET_EVENT, ESP_EVENT_ANY_ID, event, NULL);
    if (err == ESP_OK) err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL);
    if (err == ESP_OK) err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, NULL);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) goto fail;
    s_initialized = true;
    return;
fail:
    ESP_LOGE(TAG, "Network initialization failed: %s", esp_err_to_name(err));
    set_state(XLR_NET_FAILED);
}

void xlr_net_easy_connect(void) {
    if (!s_initialized || esp_event_post(XLR_NET_EVENT, REQUEST_DPP, NULL, 0, 0) != ESP_OK)
        set_state(XLR_NET_FAILED);
}
bool xlr_net_dpp_active(void) { return s_dpp_active; }
bool xlr_net_dpp_uri(char *out, size_t size) {
    if (!out || !size) return false;
    portENTER_CRITICAL(&s_dpp_lock);
    size_t len = strlen(s_dpp_uri);
    bool valid = s_dpp_active && len && len < size;
    if (valid) memcpy(out, s_dpp_uri, len + 1);
    else out[0] = 0;
    portEXIT_CRITICAL(&s_dpp_lock);
    return valid;
}
void xlr_net_provision(void) {
    if (!s_initialized || esp_event_post(XLR_NET_EVENT, REQUEST_PROVISION, NULL, 0, 0) != ESP_OK)
        set_state(XLR_NET_FAILED);
}
void xlr_net_close_provision(void) {
    if (s_initialized && esp_event_post(XLR_NET_EVENT, CLOSE_PROVISION, NULL, 0, 0) != ESP_OK)
        set_state(XLR_NET_FAILED);
}
void xlr_net_retry(void) {
    if (!s_initialized || esp_event_post(XLR_NET_EVENT, REQUEST_RETRY, NULL, 0, 0) != ESP_OK)
        set_state(XLR_NET_FAILED);
}
xlr_net_state_t xlr_net_state(void) {
    xlr_net_state_t state = s_state;
    if ((state == XLR_NET_CONNECTING || state == XLR_NET_SYNCING) && uptime() - s_attempt_at >= 60)
        return XLR_NET_FAILED;
    if (state == XLR_NET_DPP && uptime() - s_attempt_at >= 120) return XLR_NET_FAILED;
    return state;
}
bool xlr_net_time_ready(void) {
    int synced = s_synced_at;
    /* ponytail: internal clock may drift; require fresh NTP at least every 24 hours. */
    return synced >= 0 && uptime() - synced < 24 * 60 * 60;
}
bool xlr_net_ap_active(void) { return s_ap_active; }
const char *xlr_net_ap_name(void) { return s_ap; }
const char *xlr_net_ap_password(void) { return s_password; }

#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    XLR_NET_OFFLINE, XLR_NET_PROVISIONING, XLR_NET_CONNECTING,
    XLR_NET_SYNCING, XLR_NET_READY, XLR_NET_FAILED, XLR_NET_DPP
} xlr_net_state_t;

void xlr_net_start(void);
void xlr_net_provision(void);
void xlr_net_retry(void);
void xlr_net_close_provision(void);
xlr_net_state_t xlr_net_state(void);
bool xlr_net_time_ready(void);
bool xlr_net_ap_active(void);
const char *xlr_net_ap_name(void);
const char *xlr_net_ap_password(void);

void xlr_net_easy_connect(void);
bool xlr_net_dpp_active(void);
bool xlr_net_dpp_uri(char *out, size_t size);

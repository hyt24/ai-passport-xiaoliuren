#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { XLR_NET_OFFLINE, XLR_NET_PROVISIONING, XLR_NET_CONNECTING, XLR_NET_READY } xlr_net_state_t;

void xlr_net_start(void);
xlr_net_state_t xlr_net_state(void);
bool xlr_net_time_ready(void);
const char *xlr_net_ap_name(void);
const char *xlr_net_ip(void);


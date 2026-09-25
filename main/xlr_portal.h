#pragma once
#include "esp_err.h"
#include "esp_netif.h"
esp_err_t xlr_portal_start(esp_netif_t *netif);
void xlr_portal_stop(void);

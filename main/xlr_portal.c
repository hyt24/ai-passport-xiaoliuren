#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* One uncompressed IN question. Unsupported types receive a valid empty answer. */
static size_t dns_reply(uint8_t *packet, size_t len, size_t cap, const void *ip) {
    if (len < 17 || len > cap || (packet[2] & 0xf8) || packet[3] & 0x0f ||
        packet[4] || packet[5] != 1 || packet[6] || packet[7] || packet[8] || packet[9]) return 0;
    size_t pos = 12;
    while (pos < len && packet[pos]) {
        unsigned n = packet[pos++];
        if (n > 63 || n > len - pos) return 0;
        pos += n;
        if (pos - 12 > 253) return 0;
    }
    if (pos >= len || len - ++pos < 4) return 0;
    bool address = packet[pos] == 0 && packet[pos + 1] == 1;
    if (packet[pos + 2] || packet[pos + 3] != 1) return 0;
    pos += 4;
    if (address && cap - pos < 16) return 0;
    packet[2] = 0x80 | (packet[2] & 1);
    packet[3] = 0x80;
    packet[7] = address ? 1 : 0;
    packet[10] = packet[11] = 0; /* Drop EDNS/additional records. */
    if (address) {
        const uint8_t answer[] = {0xc0,12,0,1,0,1,0,0,0,0,0,4};
        memcpy(packet + pos, answer, sizeof(answer));
        memcpy(packet + pos + sizeof(answer), ip, 4);
        pos += 16;
    }
    return pos;
}

#include "xlr_portal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <stdatomic.h>
#include <unistd.h>

static atomic_bool s_running, s_alive;
static int s_socket = -1;
static uint32_t s_ip;

static void dns_task(void *arg) {
    uint8_t packet[512];
    while (s_running) {
        struct sockaddr_in peer;
        socklen_t size = sizeof(peer);
        int len = recvfrom(s_socket, packet, sizeof(packet), 0, (struct sockaddr *)&peer, &size);
        if (len <= 0) continue;
        size_t reply = dns_reply(packet, len, sizeof(packet), &s_ip);
        if (reply && s_running) sendto(s_socket, packet, reply, 0, (struct sockaddr *)&peer, size);
    }
    close(s_socket);
    s_socket = -1;
    s_alive = false;
    vTaskDelete(NULL);
}

void xlr_portal_stop(void) {
    s_running = false;
    while (s_alive) vTaskDelay(pdMS_TO_TICKS(10));
}

esp_err_t xlr_portal_start(esp_netif_t *netif) {
    if (s_alive) return ESP_OK;
    esp_netif_ip_info_t info;
    esp_err_t err = esp_netif_get_ip_info(netif, &info);
    if (err != ESP_OK) return err;
    s_ip = info.ip.addr;
    s_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_socket < 0) return ESP_FAIL;
    struct timeval timeout = {.tv_usec = 200000};
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = s_ip};
    if (setsockopt(s_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ||
        bind(s_socket, (struct sockaddr *)&addr, sizeof(addr))) {
        close(s_socket); s_socket = -1; return ESP_FAIL;
    }
    s_running = s_alive = true;
    if (xTaskCreate(dns_task, "xlr_dns", 3072, NULL, 4, NULL) != pdPASS) {
        s_running = s_alive = false;
        close(s_socket); s_socket = -1; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

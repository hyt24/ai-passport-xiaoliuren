#include "xlr_net_form.h"
#include <string.h>

static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool decode(const char *src, size_t n, char *out, size_t cap) {
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = src[i];
        if (c == '%') {
            if (i + 2 >= n || hex(src[i + 1]) < 0 || hex(src[i + 2]) < 0) return false;
            c = hex(src[i + 1]) * 16 + hex(src[i + 2]);
            i += 2;
        } else if (c == '+') c = ' ';
        if (c < 32 || c == 127 || used + 1 >= cap) return false;
        out[used++] = c;
    }
    out[used] = 0;
    return true;
}

bool xlr_net_parse_form(const char *body, char ssid[33], char password[65], char token[17]) {
    if (!body || !ssid || !password || !token) return false;
    unsigned seen = 0;
    while (*body) {
        size_t n = strcspn(body, "&");
        if (n < 2 || body[1] != '=') return false;
        unsigned bit;
        char *out;
        size_t cap;
        switch (body[0]) {
        case 's': bit = 1; out = ssid; cap = 33; break;
        case 'p': bit = 2; out = password; cap = 65; break;
        case 't': bit = 4; out = token; cap = 17; break;
        default: return false;
        }
        if ((seen & bit) || !decode(body + 2, n - 2, out, cap)) return false;
        seen |= bit;
        body += n;
        if (*body) body++;
    }
    if (seen != 7 || !ssid[0] || strlen(token) != 16) return false;
    size_t len = strlen(password);
    if (len == 64) {
        for (size_t i = 0; i < len; i++) if (hex(password[i]) < 0) return false;
    } else if (len && (len < 8 || len > 63)) return false;
    return true;
}

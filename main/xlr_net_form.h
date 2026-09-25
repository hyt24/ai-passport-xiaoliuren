#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Decode before checking byte lengths: SSIDs may contain UTF-8. */
bool xlr_net_parse_form(const char *body, char ssid[33], char password[65],
                        char token[17]);

#pragma once
#include "bsp_button.h"
#include <stdbool.h>
void xlr_app_start(bool audio_ready);
void xlr_app_key(bsp_btn_t btn, bsp_btn_ev_t ev);

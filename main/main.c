#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_pins.h"
#include "xlr_app.h"
#include "xlr_net.h"
#include "esp_log.h"

static const char *TAG = "main";

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!bsp_lvgl_lock(500)) return;
    xlr_app_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void) {
    ESP_LOGI(TAG, "AI Passport Xiaoliuren starting");
    bsp_i2c_init();
    bsp_i2c_scan();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);
    bool button_ok = bsp_button_init(on_key, NULL) == ESP_OK;
    bool audio_ok = bsp_audio_init() == ESP_OK;
    xlr_net_start();
    if (bsp_lvgl_lock(1000)) {
        xlr_app_start(audio_ok);
        bsp_lvgl_unlock();
    }
    ESP_LOGI(TAG, "ready: button=%d audio=%d", button_ok, audio_ok);
}

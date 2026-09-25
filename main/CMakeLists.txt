idf_component_register(
    SRCS "main.c"
         "xlr_app.c"
         "xlr_logic.c"
         "xlr_net.c"
         "xlr_portal.c"
         "xlr_net_form.c"
         "xlr_calendar.c"
         "lunar.c"
         "xlr_font_16.c"
         "xlr_font_display_48.c"
         "xlr_finger_guide.c"
         "xlr_paper_texture.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_wifi esp_http_server nvs_flash
    PRIV_REQUIRES esp_timer wpa_supplicant
)

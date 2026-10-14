idf_component_register(
    SRCS "main.c" "app_ui.c" "assistant.c" "chat_history.c" "text_ascii.c" "iris_screen_lvgl.c" "app_ui_camera.c" "app_ui_reply.c" "app_ui_history.c"
    INCLUDE_DIRS "."
    REQUIRES esp_iris esp-mosaico-bsp esp_mosaico_app_recovery
        app_config ai_client audio_io board_io camera_io wifi_manager web_config budgie_ui
        freertos nvs_flash
    PRIV_REQUIRES esp_timer esp_netif esp_app_format lwip heap)

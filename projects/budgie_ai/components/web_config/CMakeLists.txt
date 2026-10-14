idf_component_register(
    SRCS "web_config.c"
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "web/index.html"
    PRIV_REQUIRES esp_http_server esp_system heap app_config ai_client wifi_manager)

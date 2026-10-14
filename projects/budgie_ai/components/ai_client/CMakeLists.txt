idf_component_register(
    SRCS "ai_client.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_client esp-tls mbedtls heap)

idf_component_register(
    SRCS "camera_io.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES mosaico_module_camera esp_driver_ppa esp_mm heap)

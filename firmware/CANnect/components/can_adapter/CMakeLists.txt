idf_component_register(SRCS "can_adapter.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES  freertos esp_driver_twai)

idf_component_register(SRCS "usb_adapter.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES freertos espressif__esp_tinyusb can_adapter slcan_parser)

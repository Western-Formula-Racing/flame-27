idf_component_register( SRCS "logger.c"
                        INCLUDE_DIRS "." "../../include"
                        REQUIRES ADBMS6830 statetask measuretask esp_driver_usb_serial_jtag
)
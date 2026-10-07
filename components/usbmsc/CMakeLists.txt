idf_component_register( SRCS "usbmsc.c"
                        INCLUDE_DIRS "." "../../include"
                        REQUIRES sdcard esp_tinyusb esp_driver_usb_serial_jtag
)

idf_component_register( SRCS "ADBMS6830.c" 
                        INCLUDE_DIRS "." "../../include" 
                        REQUIRES driver isospi esp_driver_spi esp_driver_gpio
)
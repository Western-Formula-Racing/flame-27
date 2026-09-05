cmake_minimum_required(VERSION 3.16.0)

idf_component_register( SRCS "main.c" 
                        INCLUDE_DIRS "." "../components" "../include" 
                        REQUIRES "isospi" "ADBMS6830" esp_driver_spi esp_driver_gpio measuretask statetask)
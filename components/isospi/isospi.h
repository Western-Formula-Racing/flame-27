#pragma once
#include "driver/spi_master.h"
extern spi_device_handle_t spi_cs1, spi_cs2;

void SPI_Setup();
void wake_tone(spi_device_handle_t dev);
void isospi_tx(uint8_t* txData, size_t txlength, spi_device_handle_t device, bool waketone);
void isospi_tx_rx(uint8_t* txData, size_t txlength, uint8_t* rxData, size_t rxlength, spi_device_handle_t device);
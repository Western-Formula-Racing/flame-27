#pragma once
#include "driver/gpio.h"

#define FTTI 500 // Fault Tolerant Time Interval in milliseconds
#define BALANCE_THRESHOLD_V 0.01f // Voltage threshold for balancing, in volts
#define UNDERVOLT_THRESHOLD 2.7
#define OVERVOLT_THRESHOLD 4.2
#define ERROR_CHAR_BUFFER 256

#define NUM_MODULES 1
#define CELLS_PER_MODULE 13
#define THERMISTORS_PER_MODULE 10

#define MOSI_NUM GPIO_NUM_15
#define MISO_NUM GPIO_NUM_16
#define SPICLK_NUM GPIO_NUM_17
#define ISOSPI_CS GPIO_NUM_12

// Task Priorities
#define PRIO_MEASURE (configMAX_PRIORITIES - 1)
#define PRIO_LOG (configMAX_PRIORITIES - 2)
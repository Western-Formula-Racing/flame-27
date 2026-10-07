#pragma once
#include "driver/gpio.h"

#define FTTI 500 // Fault Tolerant Time Interval in milliseconds
#define CORETASK_PERIOD_MS 10
#define BALANCE_THRESHOLD_V 0.01f // Voltage threshold for balancing, in volts
#define UNDERVOLT_THRESHOLD 2.7
#define OVERVOLT_THRESHOLD 4.2

#define MAX_PEC_RETRY 10

#define NUM_MODULES 1
#define CELLS_PER_MODULE 13
#define THERMISTORS_PER_MODULE 10

//pins
//spi
#define MOSI_NUM              GPIO_NUM_6
#define MISO_NUM              GPIO_NUM_4
#define SPICLK_NUM            GPIO_NUM_5
#define ISOSPI_CS             GPIO_NUM_1
#define PIN_ADC_CS            GPIO_NUM_2
#define PIN_MICROSD_CS        GPIO_NUM_3
//digital inputs
#define PIN_BSPD_RELAY        GPIO_NUM_7
#define PIN_CHARGE_EN         GPIO_NUM_8
#define PIN_EFUSE_IMON        GPIO_NUM_9
#define PIN_AMS_RELAY         GPIO_NUM_10
#define PIN_LATCH_RELAY       GPIO_NUM_11
#define PIN_HV_ACTIVE_RELAY   GPIO_NUM_12
#define PIN_AIRN_SENSE        GPIO_NUM_13
#define PIN_IMD_RELAY         GPIO_NUM_14
#define PIN_DRDY              GPIO_NUM_41
//digital outputs
#define PIN_RTML_HSD          GPIO_NUM_15
#define PIN_TSSI_HSD          GPIO_NUM_16
#define PIN_AMS_OK            GPIO_NUM_17
#define PIN_PRECH_OK          GPIO_NUM_18
#define PIN_RED_LED           GPIO_NUM_36
#define PIN_GREEN_LED         GPIO_NUM_37
#define PIN_FAN_ENABLE        GPIO_NUM_38
//i2c
#define PIN_SCL               GPIO_NUM_39
#define PIN_SDA               GPIO_NUM_40
//can
#define PIN_CANRX             GPIO_NUM_47
#define PIN_CANTX             GPIO_NUM_48
//extra
#define PIN_GPIO_AUX1         GPIO_NUM_42
// boot mode select (strapping pin): HIGH = always log, LOW = expose SD as USB drive if a host is connected
#define PIN_MODE_SELECT       GPIO_NUM_45

// SD card
#define SD_MOUNT_POINT        "/sdcard"
#define SD_SPI_FREQ_KHZ       10000
#define USB_HOST_DETECT_MS    500

// Task Priorities
#define PRIO_CORETASK   (configMAX_PRIORITIES - 1)
#define PRIO_CANTASK    (configMAX_PRIORITIES - 2)
#define PRIO_STATETASK  (configMAX_PRIORITIES - 3)
#define PRIO_LOGTASK    (configMAX_PRIORITIES - 4)
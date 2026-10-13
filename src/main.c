#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"
#include "driver/gpio.h"

#include "isospi.h"
#include "coretask.h"
#include "statetask.h"
#include "ADBMS6830.h"
#include "LP5012.h"
#include "gpio.h"
#include "config.h"
#include "logger.h"
#include "sdcard.h"
#include "usbmsc.h"
#include "build_info.h"

#define TEST_MODE
__attribute__((used, section(".rodata.build_info")))
const char build_info[] = "BUILDINFO|" BUILD_ID "|" BUILD_USER "|" BUILD_HOST "|" BUILD_TIME;
static const char* TAG = "Main";

// Decided once at boot:
//   IO45 HIGH                -> normal (log to SD/serial), USB state ignored
//   IO45 LOW + USB host      -> USB drive mode, SD card belongs to the host
//   IO45 LOW + no USB host   -> normal
// In drive mode this never returns (reset to leave it). Returns normally in every other case.
static void enterDriveModeIfRequested(void) {
  if (readModeSelect() || !usbmsc_host_present()) {
    return;
  }
  ESP_LOGI(TAG, "USB host detected, entering drive mode");
  sdmmc_card_t* card = sdcard_open_raw();
  if (card == NULL || !usbmsc_start(card)) {
    ESP_LOGE(TAG, "Drive mode failed, continuing in normal mode");
    return;
  }
  // USB Serial/JTAG is gone from here on, so signal the mode with a fast LED blink only
  while (1) {
    setPinState(PIN_GREEN_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    setPinState(PIN_GREEN_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// mounts the SD card for firmware use. Failure is not fatal for the BMS.
static void mountSdCard(void) {
  if (!sdcard_mount()) {
    ESP_LOGW(TAG, "SD card unavailable, continuing without it");
  }
}


void app_main() {
  vTaskDelay(pdMS_TO_TICKS(2000));
  ESP_LOGI(TAG, "FLAME-27 BMS Firmware Starting...");
  ESP_LOGI(TAG, "Build-id: %s",BUILD_ID);
  esp_log_level_set("Main", ESP_LOG_DEBUG);
  esp_log_level_set("coretask", ESP_LOG_DEBUG);
  esp_log_level_set("ADBMS6830", ESP_LOG_DEBUG);
  
  // Hardware Initialization
  ESP_LOGI(TAG, "GPIO Setup...");
  GPIO_Setup();
  setPinState(PIN_RED_LED,true);
  ESP_LOGI(TAG, "SPI Setup...");
  SPI_Setup();
  ESP_LOGI(TAG, "I2C Setup...");
  setPinState(PIN_FAN_ENABLE,true);
  I2C_Setup();
  ESP_LOGI(TAG, "Init ADBMS");
  // init all ADBMS Devices with default config
  //ADBMSInit();
  //enterDriveModeIfRequested();
  //mountSdCard();
  
  // start measurement task
  //startCoreTask();
  // start State task
  
  // start CAN TX task
  
  //start logging task
  //startLoggerTask();
  
  ESP_LOGI(TAG, "Setup Complete!");
  setPinState(PIN_RED_LED,false);
  uint8_t fan_percent = 0;
  while (1) {
    ESP_LOGI(TAG,"heartbeat.");
    ESP_LOGI(TAG,"Fan Speed: %d",fan_percent);
    setFanSpeed(1,fan_percent);
    fan_percent += 25;
    setPinState(PIN_GREEN_LED,true);
    vTaskDelay(pdMS_TO_TICKS(900));
    setPinState(PIN_GREEN_LED,false);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
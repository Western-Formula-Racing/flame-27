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

#ifndef TEST_MODE
void app_main() {
  vTaskDelay(pdMS_TO_TICKS(2000));
  ESP_LOGI(TAG, "FLAME-27 BMS Firmware Starting...");
  ESP_LOGI(TAG, "Build-id: %s",BUILD_ID);
  esp_log_level_set("Main", ESP_LOG_DEBUG);
  esp_log_level_set("coretask", ESP_LOG_DEBUG);
  esp_log_level_set("ADBMS6830", ESP_LOG_DEBUG);

  // Hardware Initialization
  GPIO_Setup();
  SPI_Setup();
  enterDriveModeIfRequested();
  mountSdCard();

  // Before continuing, keep trying to read serial IDs until no PEC errors are found
  while(ADBMSReadSerialIDs() == PEC_INVALID){
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGE(TAG,"Module Not found!");
  }
  ESP_LOGI(TAG,"Module Connection Verified!");

  // configure all devices
  BMSConfig_t ADBMSConfig[NUM_MODULES];
  ADBMSGetBMSConfig(ADBMSConfig);
  for(int i = 0; i< NUM_MODULES; i++){
    ADBMSConfig[i].refon = 1;               // keep reference on after ADC conversions, uses more power but makes conversions faster
    ADBMSConfig[i].cth = 0b001;             // Allowable drift between C- and S-ADCs (8.1mV default)
    ADBMSConfig[i].soakon = 0;              // disable aux ADC soak time, test this to see if it makes an impact on thermistor readings
    ADBMSConfig[i].owrng = 0;               //short soak time range
    ADBMSConfig[i].owa = 0;                 // open wire soak time, default 32us, can set up to 500ms
    ADBMSConfig[i].gpo = 1;                 // GPIO pull downs off
    ADBMSConfig[i].fc = IIR_FILTER_5HZ;  // IIR Filter parameter
    ADBMSConfig[i].comm_bk = 0;             // if set to 1, disables communication propagation to further chips
    ADBMSConfig[i].mute_st = 1;             //if set to 1, disables discharging
    ADBMSConfig[i].snap_st = 0;             //if set to 1, activates snapshot and freezes all results registers
    ADBMSConfig[i].vuv = V_TO_UVOV(UNDERVOLT_THRESHOLD) & 0xFFF;
    ADBMSConfig[i].vov = V_TO_UVOV(OVERVOLT_THRESHOLD) & 0xFFF;
    ADBMSConfig[i].dtmen = 1;               // enable discharge timer monitor
    ADBMSConfig[i].dtrng = 1;               // set range for discharge timer (1= 16 minute increments, 0= 1 minute increments)
    ADBMSConfig[i].dcto = 8 & 0x3F;         // set for 2 hours
    ADBMSConfig[i].dcc = 0;                 // balance switches
  }
  ADBMSSetBMSConfig(ADBMSConfig);

  // start measurement task
  startCoreTask();
  // start State task

  // start CAN TX task

  //start logging task
  startLoggerTask();

  while (1) {
    ESP_LOGI(TAG,"heartbeat");
    setPinState(PIN_GREEN_LED,1);
    clearAllErrors();
    vTaskDelay(pdMS_TO_TICKS(900));
    setPinState(PIN_GREEN_LED,0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

#endif

#ifdef TEST_MODE
void app_main() {
  vTaskDelay(pdMS_TO_TICKS(2000));
  ESP_LOGI(TAG, "FLAME-27 BMS Firmware Starting...");
  ESP_LOGI(TAG, "Build-id: %s",BUILD_ID);
  GPIO_Setup();
  SPI_Setup();
  enterDriveModeIfRequested();
  mountSdCard();
  filelog();
  while (1) {
    ESP_LOGI(TAG,"heartbeat");
    setPinState(PIN_GREEN_LED,1);
    vTaskDelay(pdMS_TO_TICKS(800));
    setPinState(PIN_GREEN_LED,0);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
#endif
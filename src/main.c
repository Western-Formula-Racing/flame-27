#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"

#include "isospi.h"
#include "measuretask.h"
#include "statetask.h"
#include "ADBMS6830.h"
#include "config.h"
#include "logger.h"
#include "build_info.h"

__attribute__((used, section(".rodata.build_info")))
const char build_info[] = "BUILDINFO|" BUILD_ID "|" BUILD_USER "|" BUILD_HOST "|" BUILD_TIME;

static const char* TAG = "Main";

uint8_t AUXA[2][8];
int16_t temp_raw;
float temp_voltage;
float temp_c;
float cellVoltages[NUM_MODULES][CELLS_PER_MODULE];

void app_main() {
  vTaskDelay(pdMS_TO_TICKS(2000));
  ESP_LOGI(TAG, "FLAME-27 BMS Firmware Starting...");
  ESP_LOGI(TAG, "Build-id: %s",BUILD_ID);
  esp_log_level_set("Main", ESP_LOG_DEBUG);
  esp_log_level_set("measuretask", ESP_LOG_DEBUG);
  esp_log_level_set("ADBMS6830", ESP_LOG_DEBUG);

  // Hardware Initialization
  SPI_Setup();

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
    ADBMSConfig[i].fc = IIR_FILTER_0_625HZ;  // IIR Filter parameter
    ADBMSConfig[i].comm_bk = 0;             // if set to 1, disables communication propagation to further chips
    ADBMSConfig[i].mute_st = 1;             //if set to 1, disables discharging
    ADBMSConfig[i].snap_st = 0;             //if set to 1, activates snapshot and freezes all results registers
    ADBMSConfig[i].vuv = V_TO_UVOV(2.7) & 0xFFF;
    ADBMSConfig[i].vov = V_TO_UVOV(4.0) & 0xFFF;
    ADBMSConfig[i].dtmen = 1;               // enable discharge timer monitor
    ADBMSConfig[i].dtrng = 1;               // set range for discharge timer (1= 16 minute increments, 0= 1 minute increments)
    ADBMSConfig[i].dcto = 8 & 0x3F;         // set for 2 hours
    ADBMSConfig[i].dcc = 0;                 // balance switches
  }
  ADBMSSetBMSConfig(ADBMSConfig);

  // start measurement task
  startMeasureTask();
  //start logging task
  startLoggerTask();

  while (1) {
    ESP_LOGI(TAG,"heartbeat");
    clearAllErrors();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

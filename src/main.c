#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "isospi.h"
#include "measuretask.h"
#include "ADBMS6830.h"
#include "config.h"

static const char* TAG = "Main"; 

void app_main() {
  ESP_LOGI(TAG,"We alive");
  // Initialization
  
  // Init GPIO

  // Init PWM
  
  // Init SPI

  // Init CAN

  // Detect connected BMS

  /*
  TASKS
  Note - Measure Task execution time, stack usage
  */ 
  // Create Measurement/Safety Task

  xTaskCreatePinnedToCore(
    measureTask,    // task entrypoint
    "measureTask",  // task label
    2048,           // stack size
    NULL,           // parameters passed in
    PRIO_MEASURE,   // task priority
    NULL,           // task handle
    1               // assigned core
  );

  // Create CAN Task

  // Create State Task
  

  // Create Debug Task

  while(1){
    ESP_LOGI(TAG,"heartbeat.");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
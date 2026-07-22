#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "spi.h"
#include "ADBMS6830.h"
#include "config.h"

static const char* TAG = "Main"; 

void app_main() {

  ESP_LOGI(TAG,"We alive");
  
  esp_log_level_set("*",ESP_LOG_DEBUG);
  SPI_Setup();
  BMSConfig_t config = getBMSConfig();

  while(1){
    ESP_LOGI(TAG,"heartbeat.");
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
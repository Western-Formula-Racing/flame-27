#include "freertos/freeRTOS.h"
#include "esp_log.h"
#include "debug.h"

static const char* TAG = "debug";

void debugTask(void *pvParameters){
    TickType_t last_wake = xTaskGetTickCount();
    BaseType_t was_delayed = pdFALSE;
  while(1){
    ESP_LOGI(TAG, "Debug Info:");


    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
  }
}
#include "freertos/FreeRTOS.h"
#include "statetask.h"
#include "freertos/task.h"
#include "esp_log.h"

static uint8_t currentState;
static uint8_t lastState;

void stateTask (void *pvParameters){
  currentState = IDLE;
  lastState = IDLE;
  while(1){
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

state_e getCurrentState(){
  return currentState;
}
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "statetask.h"
#include "coretask.h"
#include "esp_log.h"

static state_e currentState;
static state_e lastState;
static Data_t stateData;

void stateTask (void *pvParameters){
  currentState = STATE_IDLE;
  lastState = STATE_IDLE;
  while(1){
    // copy data from coretask
    copyMeasureData(&stateData,pdMS_TO_TICKS(50));
    switch(currentState){
      case(STATE_IDLE):
      // repeatedly check for safety loop close
      break;
      case(STATE_PRECHARGE):
      break;
      case(STATE_HV_ACTIVE):
      break;
      case(STATE_CHARGING):
      break;
      case(STATE_BALANCING):
      break;
      case(STATE_FAULT):
      default:
      break;
    }


    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

state_e getCurrentState(){
  return currentState;
}
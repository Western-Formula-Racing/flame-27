#include "freertos/FreeRTOS.h"
#include "measuretask.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "statetask.h"

static const char* TAG = "measuretask"; 

void measureTask (void *pvParameters){
  TickType_t last_wake = xTaskGetTickCount();
  BaseType_t was_delayed = pdFALSE;

  esp_task_wdt_add(NULL);  // add task to watchdog timer, NULL = current task

  Data_t Data;
  // start ADCV continuous reading

  while(1){
    // Task Code
    if(getCurrentState() != BALANCING){
      //// if not balancing /////
      // odd open-wire ADSV
      ADBMSCommand(ADSV(0,0,0b1));
      vTaskDelay(pdMS_TO_TICKS(8));
      // even open-wire ADSV
      ADBMSCommand(ADSV(0,0,0b10));
      vTaskDelay(pdMS_TO_TICKS(8));
      // reset continuous ADSV
      ADBMSCommand(ADSV(1,0,0));
      vTaskDelay(pdMS_TO_TICKS(8));
      // read CSxFLT for ADC mismatch/ open wire (first half of register group C)
      ADBMSBroadcastRead(RDSTATC(0), (uint8_t*)Data.ADBMS_STATC, NUM_MODULES);
      // read CxOV / CxUV for over/undervolt
      ADBMSBroadcastRead(RDSTATD, (uint8_t*)Data.ADBMS_STATD, NUM_MODULES);
      // read FCxV for filtered cell voltages
      ADBMS_ReadFilteredVoltages(Data.ADBMS_filteredVoltages,NUM_MODULES,CELLS_PER_MODULE);
    } 
    else{
      ///// if balancing /////
      // Find lowest cell voltage
      // calculate array of cells to discharge
      // flip discharge switches accordingly
      // Send ADCV with RD=1, DCP=0, CONT=0 to single-shot read cell voltages (interrupts balance for 8ms) 
    }
      // read internal die temp
      // read VPV for module voltages
      // read AUX ADC for selected thermistor
      // increment selected thermistor by 1
      // read relay states
      // read external ADC for current sensor & HV voltage sense
      // check for any fault flags set
      // if fault flag matches read data, raise fault
      // additionally, manually check faults:
      // overtemp/open thermistor check
      // max cell delta check (only when idle)
      // manual OV/UV check

    // reset WDT
    esp_task_wdt_reset();
    // Delay until next cycle, raise error if not in time.
    was_delayed = xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    if(was_delayed == pdTRUE){
      ESP_LOGE(TAG, "Task Delayed!");
    }
  }
}

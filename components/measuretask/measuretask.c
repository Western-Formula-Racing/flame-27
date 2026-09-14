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
  uint8_t cyclesSinceFTTI = 0;
  esp_task_wdt_add(NULL);  // add task to watchdog timer, NULL = current task

  Data_t Data;
  uint8_t selectedThermistor = 0; // index of thermistor to read, from 0 to THERMISTORS_PER_MODULE-1
  // start ADCV continuous reading
  ADBMSCommand(ADCV(1,1,0,0,0));

  while(1){
    // Task Code
    if(getCurrentState() != BALANCING){
      //// if not balancing /////

      // Every FTTI, complete a full set of safety checks:
      if(cyclesSinceFTTI >= (FTTI/10)){
        // odd open-wire ADSV
        ADBMSCommand(ADSV(0,0,1));
        vTaskDelay(pdMS_TO_TICKS(8));
        // even open-wire ADSV
        ADBMSCommand(ADSV(0,0,2));
        vTaskDelay(pdMS_TO_TICKS(8));
        // reset continuous ADSV
        ADBMSCommand(ADSV(1,0,0));
        vTaskDelay(pdMS_TO_TICKS(8));
        // read CSxFLT for ADC mismatch/ open wire (first half of register group C)
        ADBMSBroadcastRead(RDSTATC(0), (uint8_t*)Data.ADBMS_STATC, NUM_MODULES);
        // read CxOV / CxUV for over/undervolt
        ADBMSBroadcastRead(RDSTATD, (uint8_t*)Data.ADBMS_STATD, NUM_MODULES);
        cyclesSinceFTTI = 0;
      }
      // read FCxV for filtered cell voltages
      ADBMSReadFilteredVoltages(Data.ADBMS_cellVoltages,NUM_MODULES,CELLS_PER_MODULE);
    }
    else{
      ///// if balancing /////



      //every FTTI, complete a full set of safety checks (Page 30 of ADBMS6830 datasheet):
      if(cyclesSinceFTTI >= (FTTI/10)){
        // Interrupt discharge, and compare ADCs
        ADBMSCommand(ADSV(1,0,0));
        vTaskDelay(pdMS_TO_TICKS(16)); // wait for 16ms for ADC conversions to complete
        // read all averaged C-ADC cell voltages, check max delta, and update discharge mask
        ADBMSReadAverageVoltages(Data.ADBMS_cellVoltages,NUM_MODULES,CELLS_PER_MODULE);
        Data.balanceMaxCellDelta = getMaxCellVoltageDelta(Data.ADBMS_cellVoltages,NUM_MODULES,CELLS_PER_MODULE);
        updateBalanceTargets(Data.ADBMS_cellVoltages, NUM_MODULES, CELLS_PER_MODULE, Data.dccMask, BALANCE_THRESHOLD_V);
        ADBMSCommand(ADSV(0,0,1)); // odd open-wire ADSV
        vTaskDelay(pdMS_TO_TICKS(8));
        ADBMSCommand(ADSV(0,0,2)); // even open-wire ADSV
        vTaskDelay(pdMS_TO_TICKS(8));
        // read CSxFLT for ADC mismatch/ open wire (first half of register group C)
        ADBMSBroadcastRead(RDSTATC(0), (uint8_t*)Data.ADBMS_STATC, NUM_MODULES);
        // read CxOV / CxUV for over/undervolt
        ADBMSBroadcastRead(RDSTATD, (uint8_t*)Data.ADBMS_STATD, NUM_MODULES);
        cyclesSinceFTTI = 0;
      }
    }
      // read internal die temp
      ADBMSCommand(ADAX(0,0,ADAX_CH_ITEMP));
      // read VPV for module voltages
      ADBMSCommand(ADAX(0,0,ADAX_CH_VPV));
      // read AUX ADC for selected thermistor
      ADBMSCommand(ADAX(0,0,ADAX_CH_GPIO1));
      // increment selected thermistor by 1 by adjusting GPIO pull-downs
      selectedThermistor = (selectedThermistor + 1) % THERMISTORS_PER_MODULE;
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

    // 10ms nominal period
    const TickType_t period = pdMS_TO_TICKS(10);
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - last_wake;

    // If long FTTI safety checks caused execution to overrun the 10ms window,
    // advance last_wake in integer multiples of period to maintain strict 10ms grid phase alignment
    if (elapsed >= period) {
      TickType_t missed_periods = elapsed / period;
      last_wake += missed_periods * period;
      ESP_LOGW(TAG, "FTTI/task overrun (%lu ms). Realigned grid, skipped %lu cycle(s)",
               (unsigned long)pdTICKS_TO_MS(elapsed), (unsigned long)missed_periods);
    }

    // Delay until next 10ms grid boundary
    vTaskDelayUntil(&last_wake, period);
  }
}

float updateBalanceTargets(float cellVoltages[][CELLS_PER_MODULE], uint8_t num_modules, uint8_t cells_per_module, uint8_t dccMask[][CELLS_PER_MODULE], float threshold_v) {
  if (num_modules == 0 || cells_per_module == 0) {
    return 0.0f;
  }

  // 1. Find lowest cell voltage across all modules
  float min_voltage = cellVoltages[0][0];
    for (uint8_t m = 0; m < num_modules; m++) {
        for (uint8_t c = 0; c < cells_per_module; c++) {
            if (cellVoltages[m][c] < min_voltage) {
                min_voltage = cellVoltages[m][c];
            }
        }
    }

    // 2. Populate 2D mask: 1 if cell > (min_voltage + threshold_v), else 0
    if (dccMask != NULL) {
        for (uint8_t m = 0; m < num_modules; m++) {
            for (uint8_t c = 0; c < cells_per_module; c++) {
                if (cellVoltages[m][c] > (min_voltage + threshold_v)) {
                    dccMask[m][c] = 1;
                } else {
                    dccMask[m][c] = 0;
                }
            }
        }
    }

    return min_voltage;
}

void selectTherimstor(uint8_t selectedThermistor) {
  BMSConfig_t bmsConfig = ADBMSGetBMSConfig(); // Ensure BMSConfig is up to date
  
  bmsConfig.gpo = 0; // Clear previous GPO settings
  // Set the appropriate GPO bits based on the selected thermistor
  bmsConfig.gpo |= (selectedThermistor & 0x1) << ADBMS_TMUX_A0; // Set GPO for A0
  bmsConfig.gpo |= ((selectedThermistor >> 1) & 0x1) << ADBMS_TMUX_A1; // Set GPO for A1
  bmsConfig.gpo |= ((selectedThermistor >> 2) & 0x1) << ADBMS_TMUX_A2; // Set GPO for A2
  bmsConfig.gpo |= ((selectedThermistor >> 3) & 0x1) << ADBMS_TMUX_A3; // Set GPO for A3
  
}
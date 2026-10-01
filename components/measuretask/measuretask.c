#include "freertos/FreeRTOS.h"
#include "measuretask.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "statetask.h"

//static const char* TAG = "measuretask";
TickType_t elapsed;
static Data_t Data;
static error_t Errors;

const Data_t* getMeasureData(void) {
    return &Data;
}
const error_t* getErrors(void) {
    return &Errors;
}

void clearAllErrors(){
  memset(&Errors,0,sizeof(error_t));
}
void measureTask (void *pvParameters){
  TickType_t last_wake = xTaskGetTickCount();
  uint8_t cyclesSinceFTTI = 0;
  esp_task_wdt_add(NULL);  // add task to watchdog timer, NULL = current task

  // start ADCV continuous reading
  ADBMSCommand(ADCV(1,1,0,0,0));

  TickType_t past_elapsed[5] = {0};
  uint8_t elapsed_idx = 0;

  while(1){
    // Task Code
    // ADBMS Tasks

    // read all aux ADCs
    ADBMSCommand(ADAX(0,0,0));

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
        // read CSxFLT for ADC mismatch / open wire (first half of register group C)
        ADBMSReadStat(&Data.ADBMS_STAT);
        cyclesSinceFTTI = 0;
      } else{cyclesSinceFTTI++; }
      // read FCxV for filtered cell voltages
      ADBMSReadFilteredVoltages(Data.ADBMS_filteredCellVoltages);
      // read CxV for normal voltages
      ADBMSReadVoltages(Data.ADBMS_cellVoltages);
    }
    else{
      ///// if balancing /////
      //every FTTI, complete a full set of safety checks (Page 30 of ADBMS6830 datasheet):
      if(cyclesSinceFTTI >= (FTTI/10)){
        // Interrupt discharge, and compare ADCs
        ADBMSCommand(ADSV(1,0,0));
        vTaskDelay(pdMS_TO_TICKS(16)); // wait for 16ms for ADC conversions to complete
        // read all averaged C-ADC cell voltages, check max delta, and update discharge mask
        // ADBMSReadAverageVoltages(Data.ADBMS_cellVoltages,NUM_MODULES,CELLS_PER_MODULE);
        //Data.balanceMaxCellDelta = getMaxCellVoltageDelta(Data.ADBMS_cellVoltages,NUM_MODULES,CELLS_PER_MODULE);
        updateBalanceTargets(Data.ADBMS_cellVoltages, NUM_MODULES, CELLS_PER_MODULE, Data.dccMask, BALANCE_THRESHOLD_V);
        ADBMSCommand(ADSV(0,0,1)); // odd open-wire ADSV
        vTaskDelay(pdMS_TO_TICKS(8));
        ADBMSCommand(ADSV(0,0,2)); // even open-wire ADSV
        vTaskDelay(pdMS_TO_TICKS(8));
        // read CSxFLT for ADC mismatch/ open wire (first half of register group C)
        ADBMSReadStat(&Data.ADBMS_STAT);
        cyclesSinceFTTI = 0;
      } else{cyclesSinceFTTI++; }
    }
    // read aux adc data and convert to data structure:
    ADBMSReadAux(Data.ADBMS_temps, Data.ADBMS_VMV, Data.ADBMS_moduleVoltage);
    //ESP_LOGI(TAG,"Internal die temp: %.2f",Data.ADBMS_dieTemp);
      // Motherboard Tasks:
      // read relay states
      // read external ADC for current sensor & HV voltage sense
      // check for any fault flags set
      // if fault flag matches read data, raise fault
      // additionally, manually check faults:
      // overtemp/open thermistor check
      // max cell delta check (only when idle)
      // manual OV/UV check
    // get ADBMS errors, then clear flags
    errorCheck(&Errors, Data);
    ADBMSClearAllFaults();
    // once every FTTI
    // reset WDT
    esp_task_wdt_reset();

    TickType_t current_elapsed = xTaskGetTickCount() - last_wake;
    past_elapsed[elapsed_idx] = current_elapsed;
    elapsed_idx = (elapsed_idx + 1) % 5;

    elapsed = past_elapsed[0];
    for (int i = 1; i < 5; i++) {
      if (past_elapsed[i] > elapsed) {
        elapsed = past_elapsed[i];
      }
    }

    // 10ms nominal period
    const TickType_t period = pdMS_TO_TICKS(10);
    vTaskDelayUntil(&last_wake, period);
  }
}

TickType_t getElasped(){
  return elapsed;
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

void errorCheck(error_t* errors, Data_t data){
  // go through all possible errors, trigger the right flags, add relevant info to the "description"
  uint8_t isError;
  uint32_t uvov;
  // ADBMS_UV
  for(int i=0; i<NUM_MODULES; i++){
    uvov = Data.ADBMS_STAT.D[i].UVOV;
    //process bit by bit, and trigger flag if bad
    for(int j=0; j<CELLS_PER_MODULE*2; j++){
      isError = (uvov & 1);
      uvov = uvov >> 1;
      if (j%2==0){ // if even, UV
        errors->flags |= isError << ERR_ADBMS_UV;
        errors->ADBMS_uvCell[i][j/2] |= isError;
      } else{ // if odd, OV
        errors->flags |= isError << ERR_ADBMS_OV;
        errors->ADBMS_ovCell[i][j/2] |= isError;
      }
    }
    for(int j=0; j<CELLS_PER_MODULE; j++){
    //check csflt as well
      errors->ADBMS_csFltCell[i][j] |= ((data.ADBMS_STAT.C[i].CSxFLT & (1<<i))>>i);
    }
  }
}

void startMeasureTask(){
  xTaskCreatePinnedToCore(
    measureTask,
    "measuretask",
    4096,
    NULL,
    PRIO_MEASURE,
    NULL,
    1
  );
}
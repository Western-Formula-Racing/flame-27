#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "coretask.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "statetask.h"

//static const char* TAG = "coretask";
TickType_t elapsed;
static Data_t Data, localData;
static SemaphoreHandle_t dataMutex = NULL;
static error_t Errors;

void coreTask (void *pvParameters){

  owState_e owState = OW_IDLE;
  TickType_t last_wake = xTaskGetTickCount();
  uint16_t cyclesSinceFTTI = 0;
  esp_task_wdt_add(NULL);  // add task to watchdog timer, NULL = current task

  // start ADCV continuous reading
  ADBMSCommand(ADCV(1,1,0,0,0));

  TickType_t past_elapsed[5] = {0};
  uint8_t elapsed_idx = 0;

  while(1){
    // Task Code
    cyclesSinceFTTI++;
    ////////////////////// ADBMS Tasks ///////////////////////////
    // read all aux ADCs for thermistor readings
    ADBMSCommand(ADAX(0,0,0));

    // Every FTTI, complete a full set of safety checks:
    if(cyclesSinceFTTI*CORETASK_PERIOD_MS > FTTI){
      cyclesSinceFTTI = 0;
      owState = OW_START_ODD;
    }
    switch(owState){
      case OW_START_ODD:
        // odd open-wire ADSV
        ADBMSCommand(ADSV(0,0,1));
        owState++;
        break;
      case OW_START_EVEN:
        // even open-wire ADSV
        ADBMSCommand(ADSV(0,0,2));
        owState++;
        break;
      case OW_RESTORE_CONT:
        // reset continuous ADSV
        ADBMSCommand(ADSV(1,0,0));
        owState = OW_IDLE;
        break;
      case OW_IDLE:
      default:
      // normal measurement cycle
      ADBMSReadVoltages(localData.ADBMS_cellVoltages);
      ADBMSReadFilteredVoltages(localData.ADBMS_filteredCellVoltages);
      ADBMSReadStat(&localData.ADBMS_STAT);
      // read aux adc data and convert to data structure:
      ADBMSReadAux(localData.ADBMS_temps, localData.ADBMS_VMV, localData.ADBMS_moduleVoltage);
      break;
    }
    ////////////////////// Motherboard Tasks ///////////////////////////
    // read relay states
    // read external ADC for current sensor & HV voltage sense
    // check for any fault flags set
    // if fault flag matches read data, raise fault


    // copy localData
    if(dataMutex && xSemaphoreTake(dataMutex,pdMS_TO_TICKS(5)) == pdTRUE){
      memcpy(&Data,&localData,sizeof(Data_t));
      xSemaphoreGive(dataMutex);
    }
    // get ADBMS errors, then clear flags
    errorCheck(&Errors, Data);
    ADBMSClearAllFaults();

    TickType_t current_elapsed = xTaskGetTickCount() - last_wake;
    past_elapsed[elapsed_idx] = current_elapsed;
    elapsed_idx = (elapsed_idx + 1) % 5;

    elapsed = past_elapsed[0];
    for (int i = 1; i < 5; i++) {
      if (past_elapsed[i] > elapsed) {
        elapsed = past_elapsed[i];
      }
    }

    // reset WDT
    esp_task_wdt_reset();
    // 10ms nominal period
    const TickType_t period = pdMS_TO_TICKS(10);
    vTaskDelayUntil(&last_wake, period);
  }
}

bool copyMeasureData(Data_t* out_data, TickType_t wait_ticks){
  if(!out_data || !dataMutex){
    return false;
  }
  if(xSemaphoreTake(dataMutex,wait_ticks) == pdTRUE){
    memcpy(out_data,&Data,sizeof(Data_t));
    xSemaphoreGive(dataMutex);
    return true;
  }
  return false; // timed out waiting for lock
}
const error_t* getErrors(void) {
    return &Errors;
}

void clearAllErrors(){
  memset(&Errors,0,sizeof(error_t));
}

void errorCheck(error_t* errors, Data_t data){
  // go through all possible errors, trigger the right flags, add relevant info to the "description"
  // "loud" errors in error_t should never be unset in normal operation without a power cycle
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
        errors->ADBMS_uvCellMask[i] |= (isError << (j/2));
      } else{ // if odd, OV
        errors->flags |= isError << ERR_ADBMS_OV;
        errors->ADBMS_ovCellMask[i] |= (isError << ((j/2)+1));
      }
    }
    // check CSflt on each module
    if (data.ADBMS_STAT.C[i].CSxFLT > 0){
      errors->ADBMS_csFltMask[i] |= data.ADBMS_STAT.C[i].CSxFLT;
      errors->flags |= 1 << ERR_ADBMS_CSFLT;
    }
    // manually check overvolt
    for(int j=0; j<CELLS_PER_MODULE; j++){
      if(data.ADBMS_cellVoltages[i][j] > OVERVOLT_THRESHOLD){
        errors->flags |= 1 << ERR_OVERVOLT;
        errors->overvoltCellMask[i] |= (1 << i);
      }
      // manually check undervolt
      if(data.ADBMS_cellVoltages[i][j] < UNDERVOLT_THRESHOLD){
        errors->flags |= 1 << ERR_UNDERVOLT;
        errors->undervoltCellMask[i] |= (1 << i);
      }
    }


  }
}

static void updateMinMaxVoltages(){
  for (uint8_t m = 0; m < NUM_MODULES; m++) {
    float min_voltage = 10;
    float max_voltage = 0;
    for (uint8_t c = 0; c < CELLS_PER_MODULE; c++) {
      if (Data.ADBMS_filteredCellVoltages[m][c] < min_voltage) {
        min_voltage = Data.ADBMS_filteredCellVoltages[m][c];
      }
      if (Data.ADBMS_filteredCellVoltages[m][c] > max_voltage) {
        max_voltage = Data.ADBMS_filteredCellVoltages[m][c];
      }
    }
    Data.maxCellVoltage[m] = max_voltage;
    Data.minCellVoltage[m] = min_voltage;
  }
}

void startCoreTask(){
  // create data mutex before starting task
  if(dataMutex ==  NULL){
    dataMutex = xSemaphoreCreateMutex();
  }

  xTaskCreatePinnedToCore(
    coreTask,
    "coretask",
    4096,
    NULL,
    PRIO_CORETASK,
    NULL,
    1
  );
}
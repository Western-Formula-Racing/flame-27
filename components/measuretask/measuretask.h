#pragma once
#include "config.h"
#include "ADBMS6830.h"


typedef struct{
  ADBMS_StatusGroupC_t ADBMS_STATC[NUM_MODULES];
  ADBMS_StatusGroupD_t ADBMS_STATD[NUM_MODULES];
  float    ADBMS_filteredVoltages[NUM_MODULES][CELLS_PER_MODULE];  // filtered cell voltages
  float    ADBMS_dieTemp;
  float    ADBMS_moduleVoltage;
  float    temps[THERMISTORS_PER_MODULE];
  // Relays
  uint8_t IMDRelay : 1;
  uint8_t AMSRelay : 1;
  uint8_t BSPDRelay : 1;
  uint8_t LatchRelay : 1;
  uint8_t AIRNRelay : 1;
  uint8_t HVActiveRelay : 1;
  // GPIO Outputs
  uint8_t AMSOK : 1;
  uint8_t PRECHOK : 1;
  uint8_t RTML : 1;
  uint8_t TSSI_RED : 1;
  // GPIO Inputs
  uint8_t ExpanderInt : 1;

} Data_t;

void measureTask (void *pvParameters);
#pragma once
#include "config.h"
#include "ADBMS6830.h"

typedef struct{
  float    ADBMS_cellVoltages[NUM_MODULES][CELLS_PER_MODULE];  // cell voltages
  float    ADBMS_filteredCellVoltages[NUM_MODULES][CELLS_PER_MODULE];  // filtered cell voltages
  float    ADBMS_temps[NUM_MODULES][THERMISTORS_PER_MODULE];
  float    ADBMS_moduleVoltage[NUM_MODULES];
  float    ADBMS_VMV[NUM_MODULES];
  ADBMS_Status_t ADBMS_STAT;

  //Balancing Data
  uint8_t dccMask[NUM_MODULES][CELLS_PER_MODULE]; // Discharge control mask, 1 = discharge, 0 = no discharge
  float balanceMaxCellDelta; // Max delta during paused balacing, used to determine if balancing is complete
  float balanceTargetVoltage; // Target voltage for balancing, for diagnotics only
  // Relays
  //uint8_t IMDRelay : 1;
  //uint8_t AMSRelay : 1;
  //uint8_t BSPDRelay : 1;
  //uint8_t LatchRelay : 1;
  //uint8_t AIRNRelay : 1;
  //uint8_t HVActiveRelay : 1;
  //// GPIO Outputs
  //uint8_t AMSOK : 1;
  //uint8_t PRECHOK : 1;
  //uint8_t RTML : 1;
  //uint8_t TSSI_RED : 1;
  //// GPIO Inputs
  //uint8_t ExpanderInt : 1;

} Data_t;

typedef enum {
  // "loud" errors, throw fault
  ERR_ADBMS_UV,
  ERR_ADBMS_OV,
  ERR_ADBMS_CSFLT,
  ERR_UNDERVOLT,
  ERR_OVERVOLT,
  ERR_IMBALANCE,
  ERR_CANERROR,
  ERR_CANTIMEOUT,
  ERR_LOUD_COUNT,
  // "quiet" errors, don't fault but log in error mask
  ERR_ADBMS_VAOV,
  ERR_ADBMS_VAUV,
  ERR_ADBMS_VDOV,
  ERR_ADBMS_VDUV,
  ERR_ADBMS_CMED,
  ERR_ADBMS_SMED,
  ERR_ADBMS_VDE,
  ERR_ADBMS_VDEL,
  ERR_ADBMS_SPIFLT,
  ERR_ADBMS_THSD,
} error_e;

typedef struct{
  uint32_t flags;
  uint8_t ADBMS_uvCell[NUM_MODULES][CELLS_PER_MODULE];
  uint8_t ADBMS_ovCell[NUM_MODULES][CELLS_PER_MODULE];
  uint8_t ADBMS_csFltCell[NUM_MODULES][CELLS_PER_MODULE];
  uint8_t uvCell[NUM_MODULES][CELLS_PER_MODULE];
  uint8_t ovCell[NUM_MODULES][CELLS_PER_MODULE];
  float moduleDelta[NUM_MODULES];
  float packDelta;
} error_t;

void measureTask (void *pvParameters);
//float getMaxCellVoltageDelta(float cellVoltages[][CELLS_PER_MODULE]);
float updateBalanceTargets(float cellVoltages[][CELLS_PER_MODULE], uint8_t num_modules, uint8_t cells_per_module, uint8_t dccMask[][CELLS_PER_MODULE], float threshold_v);
void errorCheck(error_t* errors, Data_t data);
void startMeasureTask();

TickType_t getElasped();
const Data_t* getMeasureData(void);
const error_t* getErrors(void);
void clearAllErrors();
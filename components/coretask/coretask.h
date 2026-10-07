#pragma once
#include "config.h"
#include "gpio.h"
#include "ADBMS6830.h"

typedef struct{
  float    ADBMS_cellVoltages[NUM_MODULES][CELLS_PER_MODULE];  // cell voltages
  float    ADBMS_filteredCellVoltages[NUM_MODULES][CELLS_PER_MODULE];  // filtered cell voltages
  float    ADBMS_temps[NUM_MODULES][THERMISTORS_PER_MODULE];
  float    ADBMS_moduleVoltage[NUM_MODULES];
  float    ADBMS_VMV[NUM_MODULES];
  ADBMS_Status_t ADBMS_STAT;

  //Balancing Data
  float maxCellVoltage[NUM_MODULES];
  float minCellVoltage[NUM_MODULES];
  uint8_t dccMask[NUM_MODULES][CELLS_PER_MODULE]; // Discharge control mask, 1 = discharge, 0 = no discharge
  float balanceMaxCellDelta; // Max delta during paused balacing, used to determine if balancing is complete
  float balanceTargetVoltage; // Target voltage for balancing, for diagnotics only
  // all GPIOs
  bool gpioStates[GPIO_MAX];
} Data_t;

typedef enum {
  OW_IDLE = 0,
  OW_START_ODD,
  OW_START_EVEN,
  OW_RESTORE_CONT,
} owState_e;

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
  uint16_t ADBMS_uvCellMask[NUM_MODULES]; // mask that stores errored UV cell state
  uint16_t ADBMS_ovCellMask[NUM_MODULES]; // mask that stores errored OV cell state
  uint8_t ADBMS_csFltMask[NUM_MODULES];   // mask that stores errored ADC mismatch Cells (open-wire indicator)
  uint8_t overvoltCellMask[NUM_MODULES];
  uint8_t undervoltCellMask[NUM_MODULES];
  float moduleDelta[NUM_MODULES];
  float packDelta;
} error_t;

void coreTask (void *pvParameters);
void errorCheck(error_t* errors, Data_t data);
void startCoreTask();

bool copyMeasureData(Data_t* out_data, TickType_t wait_ticks);
const error_t* getErrors(void);
void clearAllErrors();
#pragma once

#include "freertos/FreeRTOS.h"
#include "config.h"

//All ADBMS registers

typedef struct {
  uint8_t SIDR[6];
  uint8_t CFGAR[6];
  uint8_t CFGBR[6];
  uint8_t CVAR[6];
  uint8_t CVBR[6];
  uint8_t CVCR[6];
  uint8_t CVDR[6];
  uint8_t CVER[6];
  uint8_t CVFR[6];
  uint8_t ACVAR[6];
  uint8_t ACVBR[6];
  uint8_t ACVCR[6];
  uint8_t ACVDR[6];
  uint8_t ACVER[6];
  uint8_t ACVFR[6];
  uint8_t FCVAR[6];
  uint8_t FCVBR[6];
  uint8_t FCVCR[6];
  uint8_t FCVDR[6];
  uint8_t FCVER[6];
  uint8_t FCVFR[6];
  uint8_t SVAR[6];
  uint8_t SVBR[6];
  uint8_t SVCR[6];
  uint8_t SVDR[6];
  uint8_t SVER[6];
  uint8_t SVFR[6];
  uint8_t GPAR[6];
  uint8_t GPBR[6];
  uint8_t GPCR[6];
  uint8_t GPDR[6];
  uint8_t RGPAR[6];
  uint8_t RGPBR[6];
  uint8_t RGPCR[6];
  uint8_t RGPDR[6];
  uint8_t STAR[6];
  uint8_t STBR[6];
  uint8_t STCR[6];
  uint8_t STDR[6];
  uint8_t STER[6];
  uint8_t COMM[6];
  uint8_t PWMR[6];
  uint8_t PSR[6];
  uint8_t CMCF[6];
  uint8_t CMTC[6];
  uint8_t CMTG[6];
  uint8_t CMF[6];
  uint8_t RRR[6];
} BMSRegisters_t;


//CRC Processing functions
//void preprocess_command(uint16_t command, uint8_t* command_bytes, uint8_t* PEC_bytes);
//void preprocess_data(uint8_t* data_bytes, int length);
//uint16_t getCommandPEC(uint16_t command);
//uint16_t getDataPEC(uint8_t *pDataBuf, int nLength, int commandCounter);
//pec_t verifyRx(uint8_t *rxData);

// Direct Chip access functions
void ADBMSRead(uint16_t command, uint8_t* data);
void ADBMSReadMulti(uint16_t* command, uint8_t* data, int num_commands);
void ADBMS_Write(uint16_t command, uint8_t* data, size_t data_length);


// higher level abstraction functions
void ADBMS_Trigger_ADC();
void configureBMS(BMSConfig_t newconfig);
BMSConfig_t getBMSConfig();

// debug functions

void ADBMSRegisterDump(BMSRegisters_t* regData);
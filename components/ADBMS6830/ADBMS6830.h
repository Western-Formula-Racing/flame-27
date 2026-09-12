#pragma once

#include "freertos/FreeRTOS.h"
#include "config.h"
#include "ADBMS_defines.h"
#include "crc.h"
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

typedef struct{
  //refer to BMS maps doc for info on these
  unsigned int refon    : 1;
  unsigned int cth      : 3;
  unsigned int flag_d   : 8;
  unsigned int soakon   : 1;
  unsigned int owrng    : 1;
  unsigned int owa      : 3;
  unsigned int gpo      : 10;
  unsigned int snap_st  : 1;
  unsigned int mute_st  : 1;
  unsigned int comm_bk  : 1;
  unsigned int fc       : 3;
  unsigned int vuv      : 12;
  unsigned int vov      : 12;
  unsigned int dtmen    : 1;
  unsigned int dtrng    : 1;
  unsigned int dcto     : 6;
  unsigned int dcc      : 16;
} BMSConfig_t;

// Register group helper structs

// Register group C
typedef struct{
  uint16_t CSxFLT; // conversion mismatch fault per-cell
  uint16_t CTS;
  uint8_t SMED : 1;     // S-trim Multiple Error Detection
  uint8_t SED : 1;      // S-trim Error Detection
  uint8_t CMED : 1;     // C-trim Multiple Error Detection
  uint8_t CED : 1;      // C-trim Error Detection
  uint8_t VD_UV : 1;    // 3V Digital rail UV
  uint8_t VD_OV : 1;    // 3V Digital rail OV
  uint8_t VA_UV : 1;    // 5V Analog rail UV
  uint8_t VA_OV : 1;    // 5V Analog rail OV
  uint8_t OSCCHK : 1;   // Oscillator check
  uint8_t TMODCHK : 1;  // Test mode detection
  uint8_t THSD : 1;     // Thermal Shutdown
  uint8_t SLEEP : 1;    // Sleep mode detection
  uint8_t SPIFLT : 1;   // SPI fault detection
  uint8_t COMP : 1;     // Comparison between S-C is active
  uint8_t VDE : 1;      // Supply rail delta
  uint8_t VDEL : 1;     // Supply rail delta latent
  uint16_t PEC;         // placeholder for PEC data
} ADBMS_StatusGroupC_t;

typedef struct{
  uint32_t UVOV;    // UV/OV for each cell, alternating in that order
  uint8_t spare;
  uint8_t OC_CNTR;  // oscillator check counter, should be between 52-71
  uint16_t PEC;
} ADBMS_StatusGroupD_t;

typedef struct{
  int16_t voltage[16];
  uint16_t PEC;
} ADBMS_AllVoltageRegister_t;

//CRC Processing functions
//void preprocess_command(uint16_t command, uint8_t* command_bytes, uint8_t* PEC_bytes);
//void preprocess_data(uint8_t* data_bytes, int length);
//uint16_t getCommandPEC(uint16_t command);
//uint16_t getDataPEC(uint8_t *pDataBuf, int nLength, int commandCounter);
//pec_t verifyRx(uint8_t *rxData);

// Direct Chip access functions
void ADBMSRead(uint16_t command, uint8_t* data);
void ADBMSReadMulti(uint16_t* command, uint8_t* data, int num_commands);
void ADBMSWrite(uint16_t command, uint8_t* data, size_t data_length);
void ADBMSBroadcastWrite(uint16_t command, uint8_t* data, size_t data_length, uint8_t num_devices);
void ADBMSBroadcastRead(uint16_t command, uint8_t* data, uint8_t num_devices);
void ADBMSCommand(uint16_t command);

// higher level abstraction functions
void configureBMS(BMSConfig_t newconfig);
BMSConfig_t ADBMSGetBMSConfig();
void ADBMSReadSerialIDs(uint8_t num_modules);
void ADBMSReadFilteredVoltages(float cellVoltages[][CELLS_PER_MODULE], uint8_t num_modules, uint8_t cells_per_module);

// debug functions
void ADBMSSerialRegisterDump();
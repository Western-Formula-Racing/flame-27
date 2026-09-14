#include "ADBMS6830.h"
#include "esp_log.h"
#include "isospi.h"

static const char *TAG = "ADBMS6830";

//CRC Processing functions

// get the PEC for a command (2 bytes) - returns 2 byte PEC
// @param command 16-bit command to calculate PEC for
uint16_t getCommandPEC(uint16_t command){
  uint16_t crc = 0x0010;   //seed
  crc = command_crc15_table[(crc >> 7) ^ ((command>>8) & 0xFF)] ^ ((crc << 8) & 0x7FFF);
  crc = command_crc15_table[(crc >> 7) ^ (command & 0xFF)] ^ ((crc << 8) & 0x7FFF);
  crc = (crc << 1);        // align to 16 bits, LSB=0
  return crc;
}
// get the PEC for a data array (6 bytes) - returns 2 byte PEC
// @param pDataBuf pointer to the data array
// @param nLength length of the data array
// @param commandCounter command counter for PEC calculation
uint16_t getDataPEC(uint8_t *pDataBuf, int nLength, int commandCounter)
{
    uint16_t nRemainder = 0x10; /* PEC_SEED */
    /* x10 + x7 + x3 + x2 + x + 1 <- the CRC10 polynomial 100 1000 1111 */
    uint16_t nPolynomial = 0x8F;
    uint8_t nByteIndex, nBitIndex;
    uint16_t nTableAddr;

    for (nByteIndex = 0; nByteIndex < nLength; ++nByteIndex)
    {
        /* calculate PEC table address */
        nTableAddr = (uint16_t)(((uint16_t)(nRemainder >> 2) ^ (uint8_t)pDataBuf[nByteIndex]) & (uint8_t)0xff);
        nRemainder = (uint16_t)(((uint16_t)(nRemainder << 8)) ^ data_crc10_table[nTableAddr]);
    }
    /* If array is from received buffer add command counter to crc calculation */
    if (commandCounter != 0)
    {
        nRemainder ^= (uint16_t)(commandCounter << 4);
    }
    /* Perform modulo-2 division, a bit at a time */
    for (nBitIndex = 6u; nBitIndex > 0; --nBitIndex)
    {
        /* Try to divide the current data bit */
        if ((nRemainder & 0x200u) > 0)
        {
            nRemainder = (uint16_t)((nRemainder << 1));
            nRemainder = (uint16_t)(nRemainder ^ nPolynomial);
        }
        else
        {
            nRemainder = (uint16_t)((nRemainder << 1));
        }
    }
    return ((uint16_t)(nRemainder & 0x3FF));
}
// Calculate PEC for a command and write tjhe command and the PEC into an array.
// @param command 16-bit command to calculate PEC for
// @param command_bytes pointer to 2-byte location to write the command bytes to
// @param PEC_bytes pointer to 2-byte location to write the PEC bytes to
void preprocess_command(uint16_t command, uint8_t* command_bytes, uint8_t* PEC_bytes){
    // CMD0 = upper bits, CMD1 = lower bits
    command_bytes[0] = (command >> 8) & 0xFF;   // CMD0
    command_bytes[1] = command & 0xFF;          // CMD1

    uint16_t commandPEC = getCommandPEC(command);

    // Send PEC MSB first
    PEC_bytes[0] = (commandPEC >> 8) & 0xFF;
    PEC_bytes[1] = commandPEC & 0xFF;
}
// Preprocess an array of data and append the pEC for each 6-byte block of data. The PEC is written to the last 2 bytes of each 8-byte block.
// @param data pointer to the data array to preprocess. The array must be a multiple of 8 bytes long, with the last 2 bytes of each 8-byte block reserved for the PEC.
// @param length length of the data array in bytes. Must be a multiple of 8.
void preprocess_data(uint8_t* data, size_t length){
  
  uint16_t pec = 0;
  
  if(length % 8 !=0){
    ESP_LOGE(TAG,"Data must be in multiples of 8 bytes");
  }
  for(int i =0;i<(length/8);i++){
    pec = getDataPEC(&data[i*8],6,0);
    data[i*8+6] = (pec>>8) & 0xFF;
    data[i*8+7] = pec & 0xFF;
  }
}
// verify the PEC of a received data array. The PEC is expected to be in the last 2 bytes of each 8-byte block.
// @param rxData pointer to the received data array
// @return PEC_VALID if the PEC is valid, PEC_INVALID otherwise
pec_t verifyRx(uint8_t *rxData){
  //takes exactly one register worth of data (6 data, 2 PEC + counter bytes) and verifies the PEC.
  uint16_t calcPEC = getDataPEC(rxData,6,(rxData[6] & 0xFC)>>2);
  uint16_t recvPEC = (((rxData[6]) << 8) | rxData[7]) & 0x3FF;
  if(calcPEC == recvPEC){
    return PEC_VALID;
  }else{
    ESP_LOGE(TAG,"INVALID PEC");
    return PEC_INVALID;
  }
}
// verify the PEC of a received data array. The PEC is expected to be at the end of a 32-byte block of data.
// @param rxData pointer to the received data array
// @return PEC_VALID if the PEC is valid, PEC_INVALID otherwise
pec_t verifyRxAll(uint8_t *rxData){
  // 32 data bytes, 2 PEC bytes
  uint16_t calcPEC = getDataPEC(rxData,32,(rxData[32] & 0xFC)>>2);
  uint16_t recvPEC = (((rxData[32]) << 8) | rxData[33]) & 0x3FF;
  if(calcPEC == recvPEC){
    return PEC_VALID;
  }else{
    ESP_LOGE(TAG,"INVALID PEC");
    return PEC_INVALID;
  }
}

// Direct Chip access functions

void ADBMSRead(uint16_t command, uint8_t* data){
  // The recieved data is in the format (Data[6], PEC[2]) repeating for all chips in the chain.
  uint8_t tx_data[4] = {0};

  preprocess_command(command, &tx_data[0],&tx_data[2]);

  ESP_LOGD(TAG,"Read Command: [%X,%X,%X,%X]",tx_data[0],tx_data[1],tx_data[2],tx_data[3]);

  pec_t pecValid = PEC_INVALID;
  int attempts = 0;
  while(pecValid == PEC_INVALID && attempts < 10){
    attempts++;
    isospi_tx_rx(tx_data,4,data,8,NULL);
    pecValid = verifyRx(data);
  }

  if(pecValid == PEC_INVALID){
    ESP_LOGE(TAG,"ADBMSRead failed after %d attempts: invalid PEC", attempts);
    return;
  }

  ESP_LOGD(TAG,"RX [%X,%X,%X,%X,%X,%X,%X,%X]",data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
}

void ADBMSReadMulti(uint16_t* command, uint8_t* data, int num_commands){
  // Same as ADBMS_Read, but processes a chain of commands
  uint8_t tx_data[4] = {0};
  
  for(int i = 0; i<num_commands ;i++){
    ESP_LOGD(TAG,"Command chain %d",i);
    preprocess_command(command[i], &tx_data[0],&tx_data[2]);

    ESP_LOGD(TAG,"Read Command: [%X,%X,%X,%X]",tx_data[0],tx_data[1],tx_data[2],tx_data[3]);
    
    isospi_tx_rx(tx_data, 4, &data[i*8],8, NULL);

    ESP_LOGD(TAG,"RX [%X,%X,%X,%X,%X,%X,%X,%X]",data[0+i*8],data[1+i*8],data[2+i*8],data[3+i*8],data[4+i*8],data[5+i*8],data[6+i*8],data[7+i*8]);
  }
}

void ADBMSWrite(uint16_t command, uint8_t* data, size_t data_length){
  uint8_t command_bytes[4] = {0};
  preprocess_command(command,&command_bytes[0],&command_bytes[2]);
  preprocess_data(data,data_length);
  
  //combine command + data into one array
  uint8_t tx_data[4+data_length];
  memcpy(tx_data,command_bytes,4);
  memcpy(tx_data+4,data,data_length);
  
  isospi_tx(tx_data, 4+data_length, NULL, true);

  ESP_LOGD(TAG,"Write: Command [%X,%X,%X,%X]",command_bytes[0],command_bytes[1],command_bytes[2],command_bytes[3]);
  ESP_LOGD(TAG,"TX [%X,%X,%X,%X,%X,%X,%X,%X]",data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
}

// Write a single register across all devices
void ADBMSBroadcastWrite(uint16_t command, uint8_t* data, size_t data_length, uint8_t num_devices){
  uint8_t command_bytes[4] = {0};
  preprocess_command(command,&command_bytes[0],&command_bytes[2]);
  
  size_t total_data_length = data_length * num_devices;
  uint8_t tx_data[4+total_data_length];
  
  memcpy(tx_data,command_bytes,4);
  
  // calculate PEC for the data, and copy N times
  preprocess_data(data,data_length);
  for(int i=0; i < num_devices; i++){
    memcpy(tx_data + 4 + (i*data_length),data, data_length);
  }
  
  isospi_tx(tx_data, 4+total_data_length, NULL, true);
}

// Read a single register across all devices
void ADBMSBroadcastRead(uint16_t command, uint8_t* data, uint8_t num_devices){
  // The recieved data is in the format (Data[6], PEC[2]) repeating for all chips in the chain.
  uint8_t tx_data[4] = {0};

  preprocess_command(command, &tx_data[0],&tx_data[2]);

  pec_t pecValid = PEC_VALID;
  // recieve data, and check PEC for every block of data
  do {

    isospi_tx_rx(tx_data,4,data,8*num_devices,NULL);

    for(int i=0; i<num_devices; i++){
      if (pecValid == PEC_VALID){
        // keep going if valid, stop and retry if any invalid PEC
        pecValid = verifyRx(data+(i*8));
      }
    }
  } while(pecValid == PEC_INVALID);
}

// Write Command with no write data - Unaffected by module count
void ADBMSCommand(uint16_t command){
  uint8_t command_bytes[4] = {0};
  preprocess_command(command, &command_bytes[0], &command_bytes[2]);
  isospi_tx(command_bytes,4,NULL,true);
}

// higher level abstraction functions

void ADBMSConfigureBMS(BMSConfig_t* newconfig, uint8_t num_modules){
  for (int i = 0; i < num_modules; i++) {
    uint8_t regA_data[8] = {
      (uint8_t)(newconfig[i].refon<<7 | newconfig[i].cth),
      (uint8_t)(newconfig[i].flag_d),
      (uint8_t)((newconfig[i].soakon << 7 ) | (newconfig[i].owrng << 6) | (newconfig[i].owa << 5)),
      (uint8_t)(newconfig[i].gpo & 0xFF),
      (uint8_t)(newconfig[i].gpo >> 8),
      (uint8_t)(newconfig[i].snap_st << 5 | newconfig[i].mute_st << 4 | newconfig[i].comm_bk << 3 | newconfig[i].fc),
      0,
      0
    };
    uint8_t regB_data[8] = {
      (uint8_t)(newconfig[i].vuv & 0xFF),
      (uint8_t)(newconfig[i].vuv >> 8 | ((newconfig[i].vov << 4) & 0xF0)),
      (uint8_t)(newconfig[i].vov >> 4),
      (uint8_t)(newconfig[i].dtmen << 7 | newconfig[i].dtrng << 6 | newconfig[i].dcto),
      (uint8_t)(newconfig[i].dcc & 0xFF),
      (uint8_t)(newconfig[i].dcc >> 8),
      0,
      0
    };
    ADBMSBroadcastWrite(WRCFGA,regA_data,8,NUM_MODULES);
    ADBMSBroadcastWrite(WRCFGB,regB_data,8,NUM_MODULES);
  }
}

void ADBMSGetBMSConfig(BMSConfig_t* config, uint8_t num_modules){
  uint8_t regA_data[num_modules][8];
  uint8_t regB_data[num_modules][8];

  ADBMSReadMulti(RDCFGA,regA_data,num_modules);
  ADBMSReadMulti(RDCFGB,regB_data,num_modules);

  for (int i = 0; i < num_modules; i++) {
    config[i].refon =    (unsigned int)(regA_data[i][0] >> 7);
    config[i].cth =      (unsigned int)(regA_data[i][0] & 0x7);
    config[i].flag_d =   (unsigned int)(regA_data[i][1]);
    config[i].soakon =   (unsigned int)(regA_data[i][2] >> 7);
    config[i].owrng =    (unsigned int)((regA_data[i][2] >> 6) & 0x1);
    config[i].owa =      (unsigned int)(regA_data[i][2] >> 3 & 0x7);
    config[i].gpo =      (unsigned int)(((regA_data[i][4] & 0x3) << 8) | regA_data[i][3]);
    config[i].snap_st =  (unsigned int)(regA_data[i][5] >> 5);
    config[i].mute_st =  (unsigned int)(regA_data[i][5] >> 4);
    config[i].comm_bk =  (unsigned int)(regA_data[i][5] >> 3);
    config[i].fc =       (unsigned int)(regA_data[i][5] & 0x7);
    config[i].vuv =      (unsigned int)((regB_data[i][1] & 0xF)<<8 | regB_data[i][0]);
    config[i].vov =      (unsigned int)((regB_data[i][2]<<4) | regB_data[i][1]>>4);
    config[i].dtmen =    (unsigned int)(regB_data[i][3] >> 7);
    config[i].dtrng =    (unsigned int)(regB_data[i][3] >> 6 & 0x1);
    config[i].dcto =     (unsigned int)(regB_data[i][3] & 0x3F);
    config[i].dcc =      (unsigned int)(regB_data[i][5]<<8 | regB_data[i][4]);
  }

  //kind of annoying, need to do it all in one line because it's a macro
  for(int i = 0; i < num_modules; i++){
    ESP_LOGI(TAG,"BMS Config:\n->REFON: %d\n->CTH: %X\n->FLAG_D: %x\n->SOAKON: %d\n->OWRNG: %d\n->OWA: %X\n->GPO: %X\n->SNAP_ST: %d\n->MUTE_ST: %d\n->COMM_BK: %d\n->FC: %d\n->VUV: %X | %.4f\n->VOV: %X | %.4f\n->DTMEN: %d\n->DTRNG: %d\n->DCTO %d minutes\n->DCC: %X" ,config[i].refon,config[i].cth,config[i].flag_d,config[i].soakon, config[i].owrng, config[i].owa, config[i].gpo, config[i].snap_st, config[i].mute_st, config[i].comm_bk, config[i].fc,config[i].vuv, (float)((config[i].vuv*16*0.00015)+1.5),config[i].vov,(float)((config[i].vov*16*0.00015f)+1.5),config[i].dtmen,config[i].dtrng,config[i].dtrng ? config[i].dcto*16 : config[i].dcto*1, config[i].dcc);
  }
  return config;
}

// Read chain of Serial IDs

void ADBMSReadSerialIDs(void){
  uint8_t num_modules = NUM_MODULES;
  // tx RDSID
  uint8_t tx_data[4] = {0};
  preprocess_command(RDSID, &tx_data[0],&tx_data[2]);
  // The recieved data is in the format (Data[6], PEC[2]) repeating for all chips in the chain.
  // create a temporary buffer to store all the data
  size_t rxDataLength = num_modules*(6+2);
  uint8_t rxData[rxDataLength];

  pec_t pecValid = PEC_VALID;
  // recieve data, and check PEC for every block of data
  do {
    isospi_tx_rx(tx_data,4,rxData,rxDataLength,NULL);

    for(int i=0; i<num_modules; i++){
      if (pecValid == PEC_VALID){
        // keep going if valid, stop and retry if any invalid PEC
        pecValid = verifyRx(rxData+(i*8));
      }
    }
  } while(pecValid == PEC_INVALID);
  //log each serial ID
  for(int i=0; i<num_modules; i++){
    ESP_LOGI(TAG,"Module %d Serial ID: %02X%02X%02X%02X%02X%02X",i+1,rxData[i*8],rxData[i*8+1],rxData[i*8+2],rxData[i*8+3],rxData[i*8+4],rxData[i*8+5]);
  }
}

// Read filtered cell voltages, assuming output float array is in format [module][cell]
void ADBMSReadFilteredVoltages(float cellVoltages[][CELLS_PER_MODULE], uint8_t num_modules, uint8_t cells_per_module){
  // tx RDFCALL
  uint8_t tx_data[4] = {0};
  preprocess_command(RDFCALL, &tx_data[0],&tx_data[2]);
  // The recieved data is in the format (Data[32], PEC[2]) repeating for all chips in the chain.
  // create a temporary buffer to store all the data
  size_t rxDataLength = num_modules*(32+2);
  uint8_t rxData[rxDataLength];
  
  pec_t pecValid = PEC_VALID;
  // recieve data, and check PEC for every block of data
  do {
    isospi_tx_rx(tx_data,4,rxData,rxDataLength,NULL);

    for(int i=0; i<num_modules; i++){
      if (pecValid == PEC_VALID){
        // keep going if valid, stop and retry if any invalid PEC
        pecValid = verifyRx(rxData+(i*34));
      }
    }
  } while(pecValid == PEC_INVALID);
  //post-process data by casting to struct, converting, and writing to output
  ADBMS_AllVoltageRegister_t intVoltages[num_modules];
  memcpy(intVoltages,rxData,sizeof(ADBMS_AllVoltageRegister_t)*num_modules);
  for(int i=0; i<num_modules; i++){
      for(int j=0; j<cells_per_module; j++){
        cellVoltages[i][j] = REG_TO_V(intVoltages[i].voltage[j]);
    }
  }
}

// Read all averaged cell voltages
void ADBMSReadAverageVoltages(float cellVoltages[][CELLS_PER_MODULE], uint8_t num_modules, uint8_t cells_per_module){
  // tx RDFCALL
  uint8_t tx_data[4] = {0};
  preprocess_command(RDACALL, &tx_data[0],&tx_data[2]);
  // The recieved data is in the format (Data[32], PEC[2]) repeating for all chips in the chain.
  // create a temporary buffer to store all the data
  size_t rxDataLength = num_modules*(32+2);
  uint8_t rxData[rxDataLength];

  pec_t pecValid = PEC_VALID;
  // recieve data, and check PEC for every block of data
  do {
    isospi_tx_rx(tx_data,4,rxData,rxDataLength,NULL);

    for(int i=0; i<num_modules; i++){
      if (pecValid == PEC_VALID){
        // keep going if valid, stop and retry if any invalid PEC
        pecValid = verifyRx(rxData+(i*34));
      }
    }
  } while(pecValid == PEC_INVALID);
  //post-process data by casting to struct, converting, and writing to output
  ADBMS_AllVoltageRegister_t intVoltages[num_modules];
  memcpy(intVoltages,rxData,sizeof(ADBMS_AllVoltageRegister_t)*num_modules);
  for(int i=0; i<num_modules; i++){
      for(int j=0; j<cells_per_module; j++){
        cellVoltages[i][j] = REG_TO_V(intVoltages[i].voltage[j]);
    }
  }
}

//Debug functions

static const char* REG_NAMES[] = {
  "SIDR",
  "CFGAR","CFGBR",
  "CVAR","CVBR","CVCR","CVDR","CVER","CVFR",
  "ACVAR","ACVBR","ACVCR","ACVDR","ACVER","ACVFR",
  "FCVAR","FCVBR","FCVCR","FCVDR","FCVER","FCVFR",
  "SVAR","SVBR","SVCR","SVDR","SVER","SVFR",
  "GPAR","GPBR","GPCR","GPDR",
  "RGPAR","RGPBR","RGPCR","RGPDR",
  "STAR","STBR","STCR","STDR","STER",
  "COMM","PWMR","PSR",
  "CMCF","CMTC","CMTG","CMF",
  "RRR"
};

#define NUM_REGS (sizeof(REG_NAMES) / sizeof(REG_NAMES[0]))

void ADBMSSerialRegisterDump(void){
  BMSRegisters_t regData;

  ADBMSRead(RDSID,      &regData.SIDR[0]);
  ADBMSRead(RDCFGA,     &regData.CFGAR[0]);
  ADBMSRead(RDCFGB,     &regData.CFGBR[0]);
  ADBMSRead(RDCVA,      &regData.CVAR[0]);
  ADBMSRead(RDCVB,      &regData.CVBR[0]);
  ADBMSRead(RDCVC,      &regData.CVCR[0]);
  ADBMSRead(RDCVD,      &regData.CVDR[0]);
  ADBMSRead(RDCVE,      &regData.CVER[0]);
  ADBMSRead(RDCVF,      &regData.CVFR[0]);
  ADBMSRead(RDACA,      &regData.ACVAR[0]);
  ADBMSRead(RDACB,      &regData.ACVBR[0]);
  ADBMSRead(RDACC,      &regData.ACVCR[0]);
  ADBMSRead(RDACD,      &regData.ACVDR[0]);
  ADBMSRead(RDACE,      &regData.ACVER[0]);
  ADBMSRead(RDACF,      &regData.ACVFR[0]);
  ADBMSRead(RDFCA,      &regData.FCVAR[0]);
  ADBMSRead(RDFCB,      &regData.FCVBR[0]);
  ADBMSRead(RDFCC,      &regData.FCVCR[0]);
  ADBMSRead(RDFCD,      &regData.FCVDR[0]);
  ADBMSRead(RDFCE,      &regData.FCVER[0]);
  ADBMSRead(RDFCF,      &regData.FCVFR[0]);
  ADBMSRead(RDSVA,      &regData.SVAR[0]);
  ADBMSRead(RDSVB,      &regData.SVBR[0]);
  ADBMSRead(RDSVC,      &regData.SVCR[0]);
  ADBMSRead(RDSVD,      &regData.SVDR[0]);
  ADBMSRead(RDSVE,      &regData.SVER[0]);
  ADBMSRead(RDSVF,      &regData.SVFR[0]);
  ADBMSRead(RDAUXA,     &regData.GPAR[0]);
  ADBMSRead(RDAUXB,     &regData.GPBR[0]);
  ADBMSRead(RDAUXC,     &regData.GPCR[0]);
  ADBMSRead(RDAUXD,     &regData.GPDR[0]);
  ADBMSRead(RDRAXA,     &regData.RGPAR[0]);
  ADBMSRead(RDRAXB,     &regData.RGPBR[0]);
  ADBMSRead(RDRAXC,     &regData.RGPCR[0]);
  ADBMSRead(RDRAXD,     &regData.RGPDR[0]);
  ADBMSRead(RDSTATA,    &regData.STAR[0]);
  ADBMSRead(RDSTATB,    &regData.STBR[0]);
  ADBMSRead(RDSTATC(0), &regData.STCR[0]);
  ADBMSRead(RDSTATD,    &regData.STDR[0]);
  ADBMSRead(RDSTATE,    &regData.STER[0]);
  ADBMSRead(RDCOMM,     &regData.COMM[0]);
  ADBMSRead(RDPWMA,     &regData.PWMR[0]);
  ADBMSRead(RDPWMB,     &regData.PSR[0]);
  ADBMSRead(RDCMCFG,    &regData.CMCF[0]);
  ADBMSRead(RDCMCELLT,  &regData.CMTC[0]);
  ADBMSRead(RDCMGPIOT,  &regData.CMTG[0]);
  ADBMSRead(RDCMFLAG,   &regData.CMF[0]);
  ADBMSRead(RDRR,       &regData.RRR[0]);

  // BMSRegisters_t is 48 back-to-back uint8_t[6] members, in exactly the same
  // order as REG_NAMES above, and uint8_t has no alignment padding — so we can
  // walk regData as an array of 6-byte groups instead of a 288-arg printf.
  const uint8_t (*regs)[6] = (const uint8_t (*)[6])&regData;

  putchar('{');
  for (size_t i = 0; i < NUM_REGS; i++) {
    printf("\"%s\":[%u,%u,%u,%u,%u,%u]%s",
      REG_NAMES[i],
      regs[i][0], regs[i][1], regs[i][2], regs[i][3], regs[i][4], regs[i][5],
      (i + 1 < NUM_REGS) ? "," : "");
  }
  printf("}\n");
}
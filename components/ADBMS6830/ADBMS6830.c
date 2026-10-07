#include "ADBMS6830.h"
#include "esp_log.h"
#include "isospi.h"

static const char *TAG = "ADBMS6830";

// hold # of failed CRC checks
uint32_t PECErrorCount = 0;
// Reset PEC Error count
void resetPECErrorCount(){
  PECErrorCount = 0;
}
// Get number of failed PEC reads
// @return uint32_t PECErrorCount
uint32_t getPECErrorCount(){
  return PECErrorCount;
}
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

// Direct Chip access functions

// Write a single register across all devices
// @param command 16-bit command to write to all devices
// @param data[NUM_MODULES][8] pointer to the data array to write. The array must be of shape NUM_MODULES x 8, with the last 2 bytes of each 8-byte block reserved for the PEC. The data must come in the order of the devices in the chain.
void ADBMSBroadcastWrite(uint16_t command, uint8_t data[NUM_MODULES][8]){
  uint8_t command_bytes[4] = {0};
  preprocess_command(command,&command_bytes[0],&command_bytes[2]);

  uint8_t total_data_length = 8 * NUM_MODULES; // 8 bytes per module
  uint8_t tx_data[4+ (8 * NUM_MODULES)];
  uint8_t data_reversed[NUM_MODULES][8];

  // Make a reverse-order copy so the module chain sees the data in the correct order.
  for (int i = 0; i < NUM_MODULES; i++) {
    memcpy(data_reversed[i], data[NUM_MODULES - 1 - i], 8);
  }

  memcpy(tx_data,command_bytes,4);

  // calculate PEC for each data frame and write it into the TX data
  for(int i=0;i<NUM_MODULES;i++){
    preprocess_data(data_reversed[i],8);
    memcpy(tx_data+4+(i*8),data_reversed[i],8);
  }
  isospi_tx(tx_data, 4+total_data_length, NULL, true);
}

// Read a single register across all devices
// @param command 16-bit command to write to all devices
// @param data[NUM_MODULES][6] pointer to 2D array to write register data to.
void ADBMSBroadcastRead(uint16_t command, uint8_t data[NUM_MODULES][6]){
  // The recieved data is in the format (Data[6], PEC[2]) repeating for all chips in the chain.
  uint8_t attempts = 0;
  uint8_t tx_data[4] = {0};
  uint8_t rxData[NUM_MODULES*8];
  // calculate command PEC for command bytes
  preprocess_command(command, &tx_data[0],&tx_data[2]);
  // recieve data
  isospi_tx_rx(tx_data,4,rxData,8*NUM_MODULES,NULL);
  // keep going if valid, stop and retry if any invalid PEC
  for(int i=0; i<NUM_MODULES; i++){
    if (verifyRx(rxData+(i*8)) == PEC_INVALID && attempts < MAX_PEC_RETRY){
      ESP_LOGE(TAG, "PEC Check failed on command %d, register bytes:", command);
      ADBMSPrintRegister(rxData+(i*8));
      isospi_tx_rx(tx_data,4,rxData,8*NUM_MODULES,NULL);
      attempts++;
      PECErrorCount++;
      i = -1;
    }
  }
  // if >10 PEC attempts fail, discard data
  if(attempts >= MAX_PEC_RETRY){
    return;
  }
  // discard PEC and copy data to passed in pointer
  for(int i=0; i<NUM_MODULES; i++){
    memcpy(data[i],rxData+(i*8),6);
  }
}

// Write Command with no write data - Unaffected by module count
// @param command 16-bit command to write to all devices
void ADBMSCommand(uint16_t command){
  uint8_t command_bytes[4] = {0};
  preprocess_command(command, &command_bytes[0], &command_bytes[2]);
  isospi_tx(command_bytes,4,NULL,true);
}

// higher level abstraction functions
// Sets BMS config on all devices in the chain
// @param newconfig[NUM_MODULES] array of BMS configs to write
void ADBMSSetBMSConfig(BMSConfig_t newconfig[NUM_MODULES]){
  uint8_t regA_data[NUM_MODULES][8];
  uint8_t regB_data[NUM_MODULES][8];
  uint8_t temp[8] = {0};
  for (int i = 0; i < NUM_MODULES; i++) {
    temp[0] = (uint8_t)(newconfig[i].refon<<7 | newconfig[i].cth);
    temp[1] = (uint8_t)(newconfig[i].flag_d);
    temp[2] = (uint8_t)((newconfig[i].soakon << 7 ) | (newconfig[i].owrng << 6) | (newconfig[i].owa << 5));
    temp[3] = (uint8_t)(newconfig[i].gpo & 0xFF);
    temp[4] = (uint8_t)(newconfig[i].gpo >> 8);
    temp[5] = (uint8_t)(newconfig[i].snap_st << 5 | newconfig[i].mute_st << 4 | newconfig[i].comm_bk << 3 | newconfig[i].fc);
    memcpy(regA_data[i],temp,8*sizeof(uint8_t));
  }
  for (int i = 0; i < NUM_MODULES; i++) {
    temp[0] = (uint8_t)(newconfig[i].vuv & 0xFF);
    temp[1] = (uint8_t)(newconfig[i].vuv >> 8 | ((newconfig[i].vov << 4) & 0xF0));
    temp[2] = (uint8_t)(newconfig[i].vov >> 4);
    temp[3] = (uint8_t)(newconfig[i].dtmen << 7 | newconfig[i].dtrng << 6 | newconfig[i].dcto);
    temp[4] = (uint8_t)(newconfig[i].dcc & 0xFF);
    temp[5] = (uint8_t)(newconfig[i].dcc >> 8);
    memcpy(regB_data[i],temp,8*sizeof(uint8_t));
    }
    ADBMSBroadcastWrite(WRCFGA,regA_data);
    ADBMSBroadcastWrite(WRCFGB,regB_data);
  }
// Sets BMS config on all devices in the chain
// @param newconfig[NUM_MODULES] array of BMS configs to put read data into
void ADBMSGetBMSConfig(BMSConfig_t config[NUM_MODULES]){
  uint8_t regA_data[NUM_MODULES][6];
  uint8_t regB_data[NUM_MODULES][6];

  ADBMSBroadcastRead(RDCFGA,regA_data);
  ADBMSBroadcastRead(RDCFGB,regB_data);
  //kind of annoying, need to do it all in one line because it's a macro
  for (int i = 0; i < NUM_MODULES; i++) {
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
  for(int i = 0; i < NUM_MODULES; i++){
    ESP_LOGI(TAG,"BMS Config:\n->REFON: %d\n->CTH: %X\n->FLAG_D: %x\n->SOAKON: %d\n->OWRNG: %d\n->OWA: %X\n->GPO: %X\n->SNAP_ST: %d\n->MUTE_ST: %d\n->COMM_BK: %d\n->FC: %d\n->VUV: %X | %.4f\n->VOV: %X | %.4f\n->DTMEN: %d\n->DTRNG: %d\n->DCTO %d minutes\n->DCC: %X" ,config[i].refon,config[i].cth,config[i].flag_d,config[i].soakon, config[i].owrng, config[i].owa, config[i].gpo, config[i].snap_st, config[i].mute_st, config[i].comm_bk, config[i].fc,config[i].vuv, (float)((config[i].vuv*16*0.00015)+1.5),config[i].vov,(float)((config[i].vov*16*0.00015f)+1.5),config[i].dtmen,config[i].dtrng,config[i].dtrng ? config[i].dcto*16 : config[i].dcto*1, config[i].dcc);
  }
}

// Read chain of Serial IDs

pec_t ADBMSReadSerialIDs(){
  // tx RDSID
  uint8_t tx_data[4] = {0};
  preprocess_command(RDSID, &tx_data[0],&tx_data[2]);
  // The recieved data is in the format (Data[6], PEC[2]) repeating for all chips in the chain.
  // create a temporary buffer to store all the data
  size_t rxDataLength = NUM_MODULES*8;
  uint8_t rxData[NUM_MODULES*8];

  // recieve data, and check PEC for every block of data
  isospi_tx_rx(tx_data,4,rxData,rxDataLength,NULL);
  uint8_t pec_valid = PEC_VALID;

  for(int i=0; i<NUM_MODULES; i++){
    if(verifyRx(rxData+(i*8)) == PEC_INVALID){
      pec_valid = PEC_INVALID;
    }
  }

  //log each serial ID
  for(int i=0; i<NUM_MODULES; i++){
    ESP_LOGI(TAG,"Module %d Serial ID: %02X%02X%02X%02X%02X%02X",i+1,rxData[i*8],rxData[i*8+1],rxData[i*8+2],rxData[i*8+3],rxData[i*8+4],rxData[i*8+5]);
  }
  return pec_valid;
}

// Read filtered cell voltages, assuming output float array is in format [module][cell]
void ADBMSReadFilteredVoltages(float cellVoltages[][CELLS_PER_MODULE]){
  int16_t intVoltages[6][NUM_MODULES][3];
  ADBMSBroadcastRead(RDFCA,(uint8_t(*)[6])intVoltages[0]);
  ADBMSBroadcastRead(RDFCB,(uint8_t(*)[6])intVoltages[1]);
  ADBMSBroadcastRead(RDFCC,(uint8_t(*)[6])intVoltages[2]);
  ADBMSBroadcastRead(RDFCD,(uint8_t(*)[6])intVoltages[3]);
  ADBMSBroadcastRead(RDFCE,(uint8_t(*)[6])intVoltages[4]);
  ADBMSBroadcastRead(RDFCF,(uint8_t(*)[6])intVoltages[5]);

  // Each register contains 3 cell voltages, in order across the module.
  for (int reg = 0; reg < 6; reg++) {
    for (int module = 0; module < NUM_MODULES; module++) {
      for (int cell = 0; cell < 3; cell++) {
        int cellIndex = (reg * 3) + cell;
        if (cellIndex < CELLS_PER_MODULE) {
          cellVoltages[module][cellIndex] = REG_TO_V(intVoltages[reg][module][cell]);
        }
      }
    }
  }
}
// Read unfiltered cell voltages, assuming output float array is in format [module][cell]
void ADBMSReadVoltages(float cellVoltages[][CELLS_PER_MODULE]){
  int16_t intVoltages[6][NUM_MODULES][3];
  ADBMSBroadcastRead(RDCVA,(uint8_t(*)[6])intVoltages[0]);
  ADBMSBroadcastRead(RDCVB,(uint8_t(*)[6])intVoltages[1]);
  ADBMSBroadcastRead(RDCVC,(uint8_t(*)[6])intVoltages[2]);
  ADBMSBroadcastRead(RDCVD,(uint8_t(*)[6])intVoltages[3]);
  ADBMSBroadcastRead(RDCVE,(uint8_t(*)[6])intVoltages[4]);
  ADBMSBroadcastRead(RDCVF,(uint8_t(*)[6])intVoltages[5]);

  // Each register contains 3 cell voltages, in order across the module.
  for (int reg = 0; reg < 6; reg++) {
    for (int module = 0; module < NUM_MODULES; module++) {
      for (int cell = 0; cell < 3; cell++) {
        int cellIndex = (reg * 3) + cell;
        if (cellIndex < CELLS_PER_MODULE) {
          cellVoltages[module][cellIndex] = REG_TO_V(intVoltages[reg][module][cell]);
        }
      }
    }
  }
}

void ADBMSReadAux(float temps[NUM_MODULES][THERMISTORS_PER_MODULE], float VMV[NUM_MODULES],float VPV[NUM_MODULES]){
  int16_t intRegs[4][NUM_MODULES][3]; // Register group, module, result
  ADBMSBroadcastRead(RDAUXA,(uint8_t(*)[6])intRegs[0]);
  if(THERMISTORS_PER_MODULE>3){
  ADBMSBroadcastRead(RDAUXB,(uint8_t(*)[6])intRegs[1]);
  }
  if(THERMISTORS_PER_MODULE>6){
  ADBMSBroadcastRead(RDAUXC,(uint8_t(*)[6])intRegs[2]);
  }
  ADBMSBroadcastRead(RDAUXD,(uint8_t(*)[6])intRegs[3]);
  // each register contains 3 values. Read thermistors until THERMISTORS_PER_MODULE, then jump to VMV and VPV
  for (int reg = 0; reg < 4; reg++) {
    for (int module = 0; module < NUM_MODULES; module++) {
      for (int temp = 0; temp < 3; temp++) {
        int tempIndex = (reg * 3) + temp;
        if (tempIndex < THERMISTORS_PER_MODULE) {
          temps[module][tempIndex] = V_TO_DEGC(REG_TO_V(intRegs[reg][module][temp]));
        }
      }
    }
  }
  // read VPV and VMV
  for(int module = 0; module < NUM_MODULES; module++){
    VMV[module] = REG_TO_V(intRegs[3][module][1]);
    VPV[module] = REG_TO_V_VPV(intRegs[3][module][2]);
  }

}

void ADBMSReadStat(ADBMS_Status_t* status){
  uint16_t reg[NUM_MODULES][3];
  ADBMSBroadcastRead(RDSTATA,(uint8_t(*)[6])reg);
  for(int module = 0; module<NUM_MODULES;module++){
    status->vref2[module] = REG_TO_V(reg[module][0]);
    status->itmp[module] = REG_TO_ITMP(reg[module][1]);
  }
  ADBMSBroadcastRead(RDSTATB,(uint8_t(*)[6])reg);
  for(int module = 0; module<NUM_MODULES;module++){
    status->vd[module]   = REG_TO_V(reg[module][0]);
    status->va[module]   = REG_TO_V(reg[module][1]);
    status->vres[module] = REG_TO_V(reg[module][2]);
  }
  ADBMSBroadcastRead(RDSTATC(0),(uint8_t(*)[6])status->C);
  ADBMSBroadcastRead(RDSTATD,(uint8_t(*)[6])status->D);
}

void ADBMSClearAllFaults(){
  ADBMSCommand(CLOVUV);
  ADBMSCommand(CLRFLAG);
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

void ADBMSPrintRegister(uint8_t reg[6]){
  printf("[");
  for( int i = 0; i<6;i++){
    printf(" %X ",reg[i]);
  }
  printf("]\n");
}
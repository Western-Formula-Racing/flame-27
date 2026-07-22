#include "ADBMS6830.h"
#include "esp_log.h"

static const char *TAG = "ADBMS6830";

//CRC Processing functions
void preprocess_command(uint16_t command, uint8_t* command_bytes, uint8_t* PEC_bytes){
    // CMD0 = upper bits, CMD1 = lower bits
    command_bytes[0] = (command >> 8) & 0xFF;   // CMD0
    command_bytes[1] = command & 0xFF;          // CMD1

    uint16_t commandPEC = getCommandPEC(command);

    // Send PEC MSB first
    PEC_bytes[0] = (commandPEC >> 8) & 0xFF;
    PEC_bytes[1] = commandPEC & 0xFF;
}

void preprocess_data(uint8_t* data, int length){
  
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

uint16_t getCommandPEC(uint16_t command){
  uint16_t crc = 0x0010;   //seed
  crc = command_crc15_table[(crc >> 7) ^ ((command>>8) & 0xFF)] ^ ((crc << 8) & 0x7FFF);
  crc = command_crc15_table[(crc >> 7) ^ (command & 0xFF)] ^ ((crc << 8) & 0x7FFF);
  crc = (crc << 1);        // align to 16 bits, LSB=0
  return crc;
}

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

void ADBMSRead(uint16_t command, uint8_t* data){
  // The recieved data is in the format (Data[6], PEC[2]) repeating for all chips in the chain.
  uint8_t tx_data[4] = {0};

  preprocess_command(command, &tx_data[0],&tx_data[2]);

  ESP_LOGD(TAG,"Read Command: [%X,%X,%X,%X]",tx_data[0],tx_data[1],tx_data[2],tx_data[3]);

  pec_t pecValid = PEC_INVALID;
  while(pecValid==PEC_INVALID){
    isospi_tx_rx(tx_data,4,data,8,NULL);
    pecValid = verifyRx(data);
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

void ADBMS_Write(uint16_t command, uint8_t* data, size_t data_length){
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

// higher level abstraction functions

void ADBMS_Trigger_ADC(){
  uint8_t tx_data[4] = {0};
  preprocess_command(ADCV(0,1,0,0,0),&tx_data[0],&tx_data[2]);
  ESP_LOGD(TAG,"ADCV Command: [%X,%X,%X,%X]",tx_data[0],tx_data[1],tx_data[2],tx_data[3]);
  isospi_tx(tx_data, 4, NULL, true);
  vTaskDelay(pdMS_TO_TICKS(20));
}

void configureBMS(BMSConfig_t newconfig){
  uint8_t regA_data[8] = {
    (uint8_t)(newconfig.refon<<7 | newconfig.cth),
    (uint8_t)(newconfig.flag_d),
    (uint8_t)((newconfig.soakon << 7 ) | (newconfig.owrng << 6) | (newconfig.owa << 5)),
    (uint8_t)(newconfig.gpo & 0xFF),
    (uint8_t)(newconfig.gpo >> 8),
    (uint8_t)(newconfig.snap_st << 5 | newconfig.mute_st << 4 | newconfig.comm_bk << 3 | newconfig.fc),
    0,
    0
  };
  uint8_t regB_data[8] = {
    (uint8_t)(newconfig.vuv & 0xFF),
    (uint8_t)(newconfig.vuv >> 8 | ((newconfig.vov << 4) & 0xF0)),
    (uint8_t)(newconfig.vov >> 4),
    (uint8_t)(newconfig.dtmen << 7 | newconfig.dtrng << 6 | newconfig.dcto),
    (uint8_t)(newconfig.dcc & 0xFF),
    (uint8_t)(newconfig.dcc >> 8),
    0,
    0
  };
  ADBMS_Write(WRCFGA,regA_data,8);
  ADBMS_Write(WRCFGB,regB_data,8);
}

BMSConfig_t getBMSConfig(){
  uint8_t regA_data[8];
  uint8_t regB_data[8];

  
  ADBMSRead(RDCFGA,regA_data);
  ADBMSRead(RDCFGB,regB_data);
  
  BMSConfig_t config= {
    .refon =    (unsigned int)(regA_data[0] >> 7),
    .cth =      (unsigned int)(regA_data[0] & 0x7),
    .flag_d =   (unsigned int)(regA_data[1]),
    .soakon =   (unsigned int)(regA_data[2] >> 7),
    .owrng =    (unsigned int)((regA_data[2] >> 6) & 0x1),
    .owa =      (unsigned int)(regA_data[2] >> 3 & 0x7),
    .gpo =      (unsigned int)(((regA_data[4] & 0x3) << 8) | regA_data[3]),
    .snap_st =  (unsigned int)(regA_data[5] >> 5),
    .mute_st =  (unsigned int)(regA_data[5] >> 4),
    .comm_bk =  (unsigned int)(regA_data[5] >> 3),
    .fc =       (unsigned int)(regA_data[5] & 0x7),
    .vuv =      (unsigned int)((regB_data[1] & 0xF)<<8 | regB_data[0]),
    .vov =      (unsigned int)((regB_data[2]<<4) | regB_data[1]>>4),
    .dtmen =    (unsigned int)(regB_data[3] >> 7),
    .dtrng =    (unsigned int)(regB_data[3] >> 6 & 0x1),
    .dcto =     (unsigned int)(regB_data[3] & 0x3F),
    .dcc =      (unsigned int)(regB_data[5]<<8 | regB_data[4])
  };
  //kind of annoying, need to do it all in one line because it's a macro
  ESP_LOGI(TAG,"BMS Config:\n->REFON: %d\n->CTH: %X\n->FLAG_D: %x\n->SOAKON: %d\n->OWRNG: %d\n->OWA: %X\n->GPO: %X\n->SNAP_ST: %d\n->MUTE_ST: %d\n->COMM_BK: %d\n->FC: %d\n->VUV: %X | %.4f\n->VOV: %X | %.4f\n->DTMEN: %d\n->DTRNG: %d\n->DCTO %d minutes\n->DCC: %X",config.refon,config.cth,config.flag_d,config.soakon, config.owrng, config.owa, config.gpo, config.snap_st, config.mute_st, config.comm_bk, config.fc,config.vuv, (float)((config.vuv*16*0.00015)+1.5),config.vov,(float)((config.vov*16*0.00015f)+1.5),config.dtmen,config.dtrng,config.dtrng ? config.dcto*16 : config.dcto*1, config.dcc);
  return config;
}
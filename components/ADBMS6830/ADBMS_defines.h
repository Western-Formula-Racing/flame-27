//command definitions

#define  WRCFGA     0b00000000001  //Write Configuration Register Group A
#define  WRCFGB     0b00000100100  //Write Configuration Register Group B
#define  RDCFGA     0b00000000010  //Read Configuration Register Group A
#define  RDCFGB     0b00000100110  //Read Configuration Register Group B
#define  RDCVA      0b00000000100  //Read Cell Voltage Register Group A
#define  RDCVB      0b00000000110  //Read Cell Voltage Register Group B
#define  RDCVC      0b00000001000  //Read Cell Voltage Register Group C
#define  RDCVD      0b00000001010  //Read Cell Voltage Register Group D
#define  RDCVE      0b00000001001  //Read Cell Voltage Register Group E
#define  RDCVF      0b00000001011  //Read Cell Voltage Register Group F
#define  RDCVALL    0b00000001100  //Read All Cell Results

#define  RDACA      0b00001000100  //Read Averaged Cell Voltage Register Group A
#define  RDACB      0b00001000110  //Read Averaged Cell Voltage Register Group B
#define  RDACC      0b00001001000  //Read Averaged Cell Voltage Register Group C
#define  RDACD      0b00001001010  //Read Averaged Cell Voltage Register Group D
#define  RDACE      0b00001001001  //Read Averaged Cell Voltage Register Group E
#define  RDACF      0b00001001011  //Read Averaged Cell Voltage Register Group F
#define  RDACALL    0b00001001100  //Read All Avg Cell Results

#define  RDSVA      0b00000000011  //Read S Voltage Register Group A
#define  RDSVB      0b00000000101  //Read S Voltage Register Group B
#define  RDSVC      0b00000000111  //Read S Voltage Register Group C
#define  RDSVD      0b00000001101  //Read S Voltage Register Group D
#define  RDSVE      0b00000001110  //Read S Voltage Register Group E
#define  RDSVF      0b00000001111  //Read S Voltage Register Group F
#define  RDSALL     0b00000010000  //Read All S Results
#define  RDCSALL    0b00000010001  //Read all C and S Results
#define  RDACSALL   0b00001010001  //Read all Average C and S Results

#define  RDFCA      0b00000010010  //Read Filter Cell Voltage Register Group A
#define  RDFCB      0b00000010011  //Read Filter Cell Voltage Register Group B
#define  RDFCC      0b00000010100  //Read Filter Cell Voltage Register Group C
#define  RDFCD      0b00000010101  //Read Filter Cell Voltage Register Group D
#define  RDFCE      0b00000010110  //Read Filter Cell Voltage Register Group E
#define  RDFCF      0b00000010111  //Read Filter Cell Voltage Register Group F
#define  RDFCALL    0b00000011000  //Read All Filter Cell Results

#define  RDAUXA     0b00000011001  //Read Auxiliary Register Group A
#define  RDAUXB     0b00000011010  //Read Auxiliary Register Group B
#define  RDAUXC     0b00000011011  //Read Auxiliary Register Group C
#define  RDAUXD     0b00000011111  //Read Auxiliary Register Group D
#define  RDRAXA     0b00000011100  //Read Redundant Auxiliary Register Group A
#define  RDRAXB     0b00000011101  //Read Redundant Auxiliary Register Group B
#define  RDRAXC     0b00000011110  //Read Auxiliary Redundant Register Group C
#define  RDRAXD     0b00000100101  //Read Auxiliary Redundant Register Group D

#define  RDSTATA    0b00000110000  //Read Status Register Group A
#define  RDSTATB    0b00000110001  //Read Status Register Group B
#define  RDSTATC(ERR)   0b00000110010 | (ERR << 6)  //Read Status Register Group C
#define  RDSTATD    0b00000110011  //Read Status Register Group D
#define  RDSTATE    0b00000110100  //Read Status Register Group E
#define  RDASALL    0b00000110101  //Read all AUX/Status Registers

#define  WRPWMA     0b00000100000  //Write PWM Register Group A
#define  RDPWMA     0b00000100010  //Read PWM Register Group A
#define  WRPWMB     0b00000100001  //Write PWM Register Group B
#define  RDPWMB     0b00000100011  //Read PWM Register Group B

#define  CMDIS      0b00001000000  //LPCM Disable
#define  CMEN       0b00001000001  //LPCM Enable
#define  CMHB       0b00001000011  //LPCM Heartbeat
#define  WRCMCFG    0b00001011000  //Write LPCM Configuration Register
#define  RDCMCFG    0b00001011001  //Read LPCM Configuration Register
#define  WRCMCELLT  0b00001011010  //Write LPCM Cell Threshold
#define  RDCMCELLT  0b00001011011  //Read LPCM Cell Threshold
#define  WRCMGPIOT  0b00001011100  //Write LPCM GPIO Threshold
#define  RDCMGPIOT  0b00001011101  //Read LPCM GPIO Threshold
#define  CLRCMFLAG  0b00001011110  //Clear LPCM Flags
#define  RDCMFLAG   0b00001011111  //Read LPCM Flags

#define  ADCV(RD, CONT, DCP, RSTF, OW) (0b01001100000 | (RD << 8) | (CONT << 7) | (DCP << 4) | (RSTF<<2) | OW)  //Start Cell Voltage ADC Conversion and Poll Status
#define  ADSV(CONT, DCP, OW) (0b00101101000 | ((CONT) << 7) | ((DCP) << 4) | (OW))  //Start S-ADC Conversion and Poll Status
#define  ADAX(OW, PUP, CH) (0b10000010000 | ((OW) << 8) | ((PUP) << 7) | ((((CH) >> 4) & 0x1) << 6) | ((CH) & 0xF))  //Start AUX ADC Conversions and Poll Status
#define  ADAX2(CH) (0b10000000000 | ((CH) & 0xF))  //Start AUX2 ADC Conversions and Poll Status

#define  CLRCELL    0b11100010001  //Clear Cell Voltage Register Groups
#define  CLRFC      0b11100010100  //Clear Filtered Cell Voltage Register Groups
#define  CLRAUX     0b11100010010  //Clear Auxiliary Register Groups
#define  CLRSPIN    0b11100010110  //Clear S-Voltage Register Groups
#define  CLRFLAG    0b11100010111  //Clear Flags
#define  CLOVUV     0b11100010101  //Clear OVUV
#define  PLADC      0b11100011000  //Poll Any ADC Status
#define  PLCADC     0b11100011100  //Poll C-ADC
#define  PLSADC     0b11100011101  //Poll S-ADC
#define  PLAUX      0b11100011110  //Poll AUX ADC
#define  PLAUX2     0b11100011111  //Poll AUX2 ADC
#define  WRCOMM     0b11100100001  //Write COMM Register Group
#define  RDCOMM     0b11100100010  //Read COMM Register Group
#define  STCOMM     0b11100100011  //Start I2C/SPI Communication
#define  MUTE       0b00000101000  //Mute Discharge
#define  UNMUTE     0b00000101001  //Unmute Discharge
#define  RDSID      0b00000101100  //Read Serial ID Register Group
#define  RSTCC      0b00000101110  //Reset Command Counter
#define  SNAP       0b00000101101  //Snapshot
#define  UNSNAP     0b00000101111  //Release Snapshot
#define  SRST       0b00000100111  //Soft Reset
#define  ULRR       0b00000111000  //Unlock Retention Register
#define  WRRR       0b00000111001  //Write Retention Registers
#define  RDRR       0b00000111010  //Read Retention Registers

// From Table 21 of datasheet
#define IIR_FILTER_DISABLED 0
#define IIR_FILTER_110HZ    1
#define IIR_FILTER_45HZ     2
#define IIR_FILTER_21HZ     3
#define IIR_FILTER_10HZ     4
#define IIR_FILTER_5HZ      5
#define IIR_FILTER_1_25HZ   6
#define IIR_FILTER_0_625HZ  7

// macros
#define REG_TO_V(value) ((float)value * 0.00015f) + 1.5f
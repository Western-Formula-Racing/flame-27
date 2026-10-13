#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"

typedef enum {
  PWM_OUT1 = 0x0C,
  PWM_OUT2,
  PWM_OUT3,
  PWM_OUT4,
  PWM_OUT5,
  PWM_OUT6,
  PWM_OUT7,
  PWM_OUT8,
  PWM_OUT9,
  PWM_OUT10,
}pwmChannel_e;


//setup I2C for PWM driver
void I2C_Setup(void);
//set fan speed from 0 to 255
// @param fan fan number from 1 to 10
// @param speed fan speed from 0 to 255
void setFanSpeed(int fan, uint8_t speed);
#include "LP5012.h"
#include "config.h"
#include "esp_log.h"

#define DEVICE_CONFIG0  0x0
#define DEVICE_CONFIG1  0x1
#define LED_CONFIG0     0x2
#define LED0_BRIGHTNESS 0x7
#define LED1_BRIGHTNESS 0x8
#define LED2_BRIGHTNESS 0x9
#define LED3_BRIGHTNESS 0xA

static i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t pwm_dev_handle;
static uint8_t tx_data_buffer[2];

static const char* TAG = "LP5012";

void I2C_Setup(void){
  i2c_master_bus_config_t i2cConfig = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1, //auto-select
    .scl_io_num = PIN_SCL,
    .sda_io_num = PIN_SDA,
    .flags.enable_internal_pullup = GPIO_PULLUP_ENABLE,
    .glitch_ignore_cnt = 7
  };
  ESP_LOGI(TAG,"Initializing I2C Bus...");
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2cConfig,&i2c_bus_handle));
  
  i2c_device_config_t pwm_dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_7,
    .device_address = 0b0010100,
    .scl_speed_hz = 20 * 1000, // 20kHz
  };
  
  ESP_LOGI(TAG,"Adding PWM Driver to I2C Bus");
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle,&pwm_dev_cfg,&pwm_dev_handle));
  
  ESP_LOGI(TAG,"Programming default PWM driver values...");
  // chip enable
  tx_data_buffer[0] = DEVICE_CONFIG0;
  tx_data_buffer[1] = 0b01000000;
  i2c_master_transmit(pwm_dev_handle,tx_data_buffer,2,10);
  // disable unused options
  tx_data_buffer[0] = DEVICE_CONFIG1;
  tx_data_buffer[1] = 0b00000100;
  i2c_master_transmit(pwm_dev_handle,tx_data_buffer,2,10);
  // disable LED banks
  tx_data_buffer[0] = LED_CONFIG0;
  tx_data_buffer[1] = 0b00000000;
  i2c_master_transmit(pwm_dev_handle,tx_data_buffer,2,10);
  // set all brightness to 100
  for(int i = 0; i<3;i++){
    tx_data_buffer[0] = LED0_BRIGHTNESS+i;
    tx_data_buffer[1] = 0xFF;
    i2c_master_transmit(pwm_dev_handle,tx_data_buffer,2,10);
  }
  // set individual PWMs to 0
  for(int i=0;i<10;i++){
    tx_data_buffer[0] = PWM_OUT1+i;
    tx_data_buffer[1] = 0x00;
    i2c_master_transmit(pwm_dev_handle,tx_data_buffer,2,10);
  }
}

void setFanSpeed(int fan, uint8_t speed){
  tx_data_buffer[0] = PWM_OUT1+fan-1;
  tx_data_buffer[1] = 255U - speed;
  i2c_master_transmit(pwm_dev_handle,tx_data_buffer,2,10);
}
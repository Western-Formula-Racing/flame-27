#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "config.h"
#include "isospi.h"
#include "esp_log.h"


static const char* TAG = "isospi"; 

spi_device_handle_t spi_cs1, spi_cs2;

void SPI_Setup(){
  esp_err_t err;
  esp_log_level_set(TAG,ESP_LOG_INFO);
  spi_bus_config_t bus_conf = { 
    .mosi_io_num = MOSI_NUM,
    .miso_io_num = MISO_NUM,
    .sclk_io_num = SPICLK_NUM,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .max_transfer_sz = 4096,
    .flags = 0,
    .isr_cpu_id = INTR_CPU_ID_AUTO,
    .intr_flags = 0,
  };

  err = spi_bus_initialize (SPI2_HOST, &bus_conf, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) ESP_LOGE (TAG, "Failed to Initialise SPI");

  spi_device_interface_config_t devcfg_cs1 = { 
    .mode = 3,
    .clock_speed_hz = 1 * 1000 * 100, // 500KHz
    .spics_io_num = CS_NUM,
    .flags = 0,
    .queue_size = 7,
  };

  ESP_ERROR_CHECK(spi_bus_add_device (SPI2_HOST, &devcfg_cs1, &spi_cs1));
  ESP_LOGI(TAG, "CS1 flags = 0x%08x", ((unsigned)devcfg_cs1.flags));

  ESP_LOGI(TAG, "SPI Sender and Receiver are Initilialised");
}

void wake_tone(spi_device_handle_t dev){

  uint8_t wake_signal[] = { 0xFF, 0x22 };
  spi_transaction_t wake = {
    .length = sizeof (wake_signal) * 8,
    .tx_buffer = &wake_signal,
  };
  //For 240us, continously send the wake up signal (8 times in total)
  for (int i = 0; i < 8; i++)
	{
    if(dev==NULL){
      spi_device_polling_transmit (spi_cs1, &wake);
    } else{
      spi_device_polling_transmit (dev, &wake);
    }
	}
    esp_rom_delay_us (10);
}

void isospi_tx(uint8_t* txData, size_t txlength, spi_device_handle_t device, bool waketone){
  if(device==NULL){
    device = spi_cs1;
  }
  //TX Command
  spi_transaction_t t_tx = {
    .flags = 0,
    .cmd = 0,
    .addr = 0,
    .length = txlength*8,
    .rxlength = 0,
    .user = NULL,
    .tx_buffer = txData,
    .rx_buffer = 0,
  };
  spi_device_acquire_bus(device, portMAX_DELAY); // lock bus for this transaction
  
  if(waketone){
    wake_tone(device); // wake up isoSPI interface
  }

  esp_err_t err = spi_device_polling_transmit(device, &t_tx);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "SPI Error");
  }

  spi_device_release_bus(device); 
}

void isospi_tx_rx(uint8_t* txData, size_t txlength, uint8_t* rxData, size_t rxlength, spi_device_handle_t device){

  if(device==NULL){
    device = spi_cs1;
  }
  esp_err_t err;
  
  //TX Command
  spi_transaction_t t_tx = {
    .flags = SPI_TRANS_CS_KEEP_ACTIVE,
    .cmd = 0,
    .addr = 0,
    .length = txlength*8,
    .rxlength = 0,
    .user = NULL,
    .tx_buffer = txData,
    .rx_buffer = 0,
  };
  //RX Command
  spi_transaction_t t_rx = {
    .flags = 0,
    .cmd = 0,
    .addr = 0,
    .length = rxlength*8,
    .rxlength = rxlength*8,
    .user = NULL,
    .tx_buffer = 0,
    .rx_buffer = rxData,
  };
  //transmit and recieve
  spi_device_acquire_bus(device, portMAX_DELAY); // lock bus for this transaction
  wake_tone(device); // wake up isoSPI interface
  err = spi_device_polling_transmit(device, &t_tx);
  err = spi_device_polling_transmit(device, &t_rx);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "SPI Error");
  }
  spi_device_release_bus(device); 
}
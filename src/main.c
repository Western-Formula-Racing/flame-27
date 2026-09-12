#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "isospi.h"
#include "measuretask.h"
#include "ADBMS6830.h"
#include "config.h"
#include "driver/usb_serial_jtag.h"

static const char* TAG = "Main"; 

static uint8_t module_number = 1; // default until serial input changes it

static void update_module_number_from_serial(void)
{
    uint8_t rx_buf[16];
    int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf) - 1, 0);
    if (len > 0) {
        rx_buf[len] = '\0';
        int new_module = atoi((char *)rx_buf);
        if (new_module > 0 && new_module <= 255) {
            module_number = (uint8_t)new_module;
            ESP_LOGI(TAG, "Module number updated to %d", module_number);
        }
    }
}

void app_main() {
  ESP_LOGI(TAG,"We alive");

  usb_serial_jtag_driver_config_t usj_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  esp_err_t err = usb_serial_jtag_driver_install(&usj_config);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to install USB Serial/JTAG driver: %s", esp_err_to_name(err));
  }
  // Initialization
  
  // Init GPIO

  // Init PWM
  
  // Init SPI

  // Init CAN

  // Detect connected BMS

  /*
  TASKS
  Note - Measure Task execution time, stack usage
  */ 
  // Create Measurement/Safety Task
  /*
  xTaskCreatePinnedToCore(
    measureTask,    // task entrypoint
    "measureTask",  // task label
    2048,           // stack size
    NULL,           // parameters passed in
    PRIO_MEASURE,   // task priority
    NULL,           // task handle
    1               // assigned core
  );
  */
  
  // Create CAN Task

  // Create State Task
  

  // Create Debug Task

  //BRINGUP CODE

  // Sanity Check - Read ADBMS chip ID
  SPI_Setup();
  while (1) {
      ESP_LOGI(TAG, "heartbeat.");

      update_module_number_from_serial();
      ADBMSReadSerialIDs(module_number);

      vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
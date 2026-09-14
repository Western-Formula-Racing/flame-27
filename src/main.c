#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"

#include "isospi.h"
#include "measuretask.h"
#include "statetask.h"
#include "ADBMS6830.h"
#include "config.h"
#include "cli_tool.h"

static const char* TAG = "Main"; 

void app_main() {
    ESP_LOGI(TAG, "FLAME-27 BMS Firmware Starting...");

    // Install USB Serial JTAG Driver for CLI & debug output
    usb_serial_jtag_driver_config_t usj_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t err = usb_serial_jtag_driver_install(&usj_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Serial/JTAG driver: %s", esp_err_to_name(err));
    }

    // Initialize CLI and launch CLI task
    cli_init();
    cli_start_task(5, 4096);

    // Hardware Initialization
    SPI_Setup();

    while (1) {
        ESP_LOGI(TAG, "Heartbeat. Querying %d module(s)", NUM_MODULES);

        ADBMSReadSerialIDs();

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

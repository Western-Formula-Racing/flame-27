#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"

#include "usbmsc.h"
#include "config.h"

static const char* TAG = "usbmsc";

bool usbmsc_host_present(void){
  // the connection monitor assumes "connected" until it has gone a few ticks without SOF packets from a host,
  // so give it time to settle before reading
  vTaskDelay(pdMS_TO_TICKS(USB_HOST_DETECT_MS));
  return usb_serial_jtag_is_connected();
}

bool usbmsc_start(sdmmc_card_t* card){
  if(card == NULL){
    return false;
  }

  // the host owns the card for the whole session, so never remount it to the app on connect/disconnect
  const tinyusb_msc_driver_config_t driverCfg = {
    .user_flags.auto_mount_off = 1,
  };
  esp_err_t err = tinyusb_msc_install_driver(&driverCfg);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "MSC driver install failed (%s)", esp_err_to_name(err));
    return false;
  }

  const tinyusb_msc_storage_config_t storageCfg = {
    .medium = { .card = card },
    .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
  };
  tinyusb_msc_storage_handle_t storage = NULL;
  err = tinyusb_msc_new_storage_sdmmc(&storageCfg, &storage);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "MSC storage create failed (%s)", esp_err_to_name(err));
    tinyusb_msc_uninstall_driver();
    return false;
  }

  const tinyusb_config_t tusbCfg = TINYUSB_DEFAULT_CONFIG();
  err = tinyusb_driver_install(&tusbCfg);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "TinyUSB install failed (%s)", esp_err_to_name(err));
    tinyusb_msc_delete_storage(storage);
    tinyusb_msc_uninstall_driver();
    return false;
  }
  ESP_LOGI(TAG, "SD card exposed over USB");
  return true;
}

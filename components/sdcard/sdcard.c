#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"

#include "sdcard.h"
#include "config.h"

static const char* TAG = "sdcard";

static sdmmc_card_t* card = NULL;
static sdspi_dev_handle_t rawHandle;
static bool mounted = false;

#define LOG_NAME_LEN 11   // strlen("LOGXXXX.csv")
#define LOG_MAX_NUM  9999
static char logPath[sizeof(SD_MOUNT_POINT) + LOG_NAME_LEN + 1] = "";

// The SD socket has no external pull-ups on MISO/CS/MOSI, which SD cards in SPI mode need (a floating MISO
// reads as "no response" during init). Enable the ESP32's weaker internal ones (~45k) as a stopgap until
// 10k resistors are added on the board. Safe on the shared bus: the pins are only ever driven push-pull.
static void enableBusPullups(void){
  gpio_set_pull_mode(MISO_NUM, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(MOSI_NUM, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(PIN_MICROSD_CS, GPIO_PULLUP_ONLY);
}

// releases a card opened with sdcard_open_raw()
static void releaseRaw(void){
  if(card == NULL || mounted){
    return;
  }
  free(card);
  card = NULL;
  sdspi_host_remove_device(rawHandle);
  sdspi_host_deinit();
}

bool sdcard_mount(void){
  if(mounted){
    return true;
  }
  releaseRaw(); // e.g. USB drive mode failed to start after the raw card was opened
  enableBusPullups();
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.max_freq_khz = SD_SPI_FREQ_KHZ;

  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.host_id = SPI2_HOST;
  slot.gpio_cs = PIN_MICROSD_CS;

  esp_vfs_fat_mount_config_t mountCfg = {
    .format_if_mount_failed = false, // never wipe a card automatically
    .max_files = 4,
    .allocation_unit_size = 16 * 1024,
  };

  esp_err_t err = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot, &mountCfg, &card);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "Failed to mount SD card (%s)", esp_err_to_name(err));
    card = NULL;
    return false;
  }
  mounted = true;
  ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
  return true;
}

void sdcard_unmount(void){
  if(!mounted){
    return;
  }
  esp_err_t err = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "Failed to unmount SD card (%s)", esp_err_to_name(err));
    return;
  }
  card = NULL;
  mounted = false;
  logPath[0] = '\0';
  ESP_LOGI(TAG, "SD card unmounted");
}

sdmmc_card_t* sdcard_open_raw(void){
  if(mounted){
    ESP_LOGE(TAG, "Cannot open raw while the filesystem is mounted");
    return NULL;
  }
  if(card != NULL){
    return card;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.max_freq_khz = SD_SPI_FREQ_KHZ;

  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.host_id = SPI2_HOST;
  slot.gpio_cs = PIN_MICROSD_CS;

  enableBusPullups();
  sdspi_dev_handle_t handle;
  esp_err_t err = sdspi_host_init();
  if(err != ESP_OK){
    ESP_LOGE(TAG, "sdspi_host_init failed (%s)", esp_err_to_name(err));
    return NULL;
  }
  err = sdspi_host_init_device(&slot, &handle);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "sdspi_host_init_device failed (%s)", esp_err_to_name(err));
    sdspi_host_deinit();
    return NULL;
  }
  host.slot = handle;
  enableBusPullups(); // adding the device reconfigures the CS pin

  sdmmc_card_t* newCard = malloc(sizeof(sdmmc_card_t));
  if(newCard == NULL){
    sdspi_host_remove_device(handle);
    sdspi_host_deinit();
    return NULL;
  }
  err = sdmmc_card_init(&host, newCard);
  if(err != ESP_OK){
    ESP_LOGE(TAG, "SD card init failed (%s)", esp_err_to_name(err));
    free(newCard);
    sdspi_host_remove_device(handle);
    sdspi_host_deinit();
    return NULL;
  }
  rawHandle = handle;
  card = newCard;
  return card;
}

bool sdcard_is_mounted(void){
  return mounted;
}

// true if name is exactly LOGXXXX.csv (X = digit, case-insensitive); writes the number to *num
static bool parseLogName(const char* name, int* num){
  if(strlen(name) != LOG_NAME_LEN || strncasecmp(name, "LOG", 3) != 0 || strcasecmp(name + 7, ".csv") != 0){
    return false;
  }
  int value = 0;
  for(int i = 3; i < 7; i++){
    if(!isdigit((unsigned char)name[i])){
      return false;
    }
    value = value * 10 + (name[i] - '0');
  }
  *num = value;
  return true;
}

bool filelog(void){
  if(!mounted){
    ESP_LOGE(TAG, "Cannot create log file, SD card not mounted");
    return false;
  }

  DIR* dir = opendir(SD_MOUNT_POINT);
  if(dir == NULL){
    ESP_LOGE(TAG, "Cannot open %s", SD_MOUNT_POINT);
    return false;
  }
  int highest = 0;
  struct dirent* entry;
  while((entry = readdir(dir)) != NULL){
    int num;
    if(parseLogName(entry->d_name, &num) && num > highest){
      highest = num;
    }
  }
  closedir(dir);

  if(highest >= LOG_MAX_NUM){
    ESP_LOGE(TAG, "All %d log file numbers are used", LOG_MAX_NUM);
    return false;
  }

  char path[sizeof(logPath)];
  snprintf(path, sizeof(path), "%s/LOG%04u.csv", SD_MOUNT_POINT, (unsigned)(highest + 1) % 10000u);
  FILE* f = fopen(path, "w");
  if(f == NULL){
    ESP_LOGE(TAG, "Cannot create %s", path);
    return false;
  }
  fclose(f);

  strcpy(logPath, path);
  ESP_LOGI(TAG, "Logging to %s", logPath);
  return true;
}

const char* sdcard_log_path(void){
  return logPath[0] != '\0' ? logPath : NULL;
}
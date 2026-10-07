#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "gpio.h"
#include "config.h"

static SemaphoreHandle_t gpioMutex = NULL;

const gpioInfo_t gpioInfo[GPIO_MAX] = {
  [GPIO_BSPD]    = { PIN_BSPD_RELAY,      GPIO_DIR_IN,  "BSPD"    },
  [GPIO_CHARGE]  = { PIN_CHARGE_EN,       GPIO_DIR_OUT, "CHARGE"  },
  [GPIO_AMS]     = { PIN_AMS_RELAY,       GPIO_DIR_OUT, "AMS"     },
  [GPIO_LATCH]   = { PIN_LATCH_RELAY,     GPIO_DIR_IN,  "LATCH"   },
  [GPIO_HVACT]   = { PIN_HV_ACTIVE_RELAY, GPIO_DIR_IN,  "HVACT"   },
  [GPIO_AIRN]    = { PIN_AIRN_SENSE,      GPIO_DIR_IN,  "AIRN"    },
  [GPIO_IMD]     = { PIN_IMD_RELAY,       GPIO_DIR_IN,  "IMD"     },
  [GPIO_DRDY]    = { PIN_DRDY,            GPIO_DIR_IN,  "DRDY"    },
  [GPIO_RTML]    = { PIN_RTML_HSD,        GPIO_DIR_OUT, "RTML"    },
  [GPIO_TSSI]    = { PIN_TSSI_HSD,        GPIO_DIR_OUT, "TSSI"    },
  [GPIO_AMSOK]   = { PIN_AMS_OK,          GPIO_DIR_OUT, "AMSOK"   },
  [GPIO_PRECHOK] = { PIN_PRECH_OK,        GPIO_DIR_OUT, "PRECHOK" },
  [GPIO_RED]     = { PIN_RED_LED,         GPIO_DIR_OUT, "RED"     },
  [GPIO_GREEN]   = { PIN_GREEN_LED,       GPIO_DIR_OUT, "GREEN"   },
  [GPIO_FANEN]   = { PIN_FAN_ENABLE,      GPIO_DIR_OUT, "FANEN"   },
};

void GPIO_Setup(void) {
  if(gpioMutex ==  NULL){
    gpioMutex = xSemaphoreCreateMutex();
  }

  uint64_t outMask = 0;
  uint64_t inMask = 0;
  for (int i = 0; i < GPIO_MAX; i++) {
    if (gpioInfo[i].dir == GPIO_DIR_OUT) {
      gpio_set_level(gpioInfo[i].pinIndex, 0); // latch LOW before enabling the driver so nothing glitches on at boot
      outMask |= 1ULL << gpioInfo[i].pinIndex;
    } else {
      inMask |= 1ULL << gpioInfo[i].pinIndex;
    }
  }

  // outputs are INPUT_OUTPUT so their state can be read back
  gpio_config_t outConf = {
    .pin_bit_mask = outMask,
    .mode = GPIO_MODE_INPUT_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&outConf));

  gpio_config_t inConf = {
    .pin_bit_mask = inMask,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&inConf));

  // Chip selects of devices on the shared SPI bus that firmware doesn't drive yet. Left floating they can
  // answer on MISO and corrupt other devices' transfers (e.g. SD card init), so park them deselected (HIGH).
  gpio_set_level(PIN_ADC_CS, 1);
  gpio_config_t csConf = {
    .pin_bit_mask = 1ULL << PIN_ADC_CS,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&csConf));
}

bool readModeSelect(void){
  // internal pull-down so an unconnected pin reads LOW
  gpio_config_t conf = {
    .pin_bit_mask = 1ULL << PIN_MODE_SELECT,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&conf));
  esp_rom_delay_us(100); // let the pull-down settle against any external network
  return gpio_get_level(PIN_MODE_SELECT) != 0;
}

bool getPinState(uint8_t pin, bool* status){
  if(xSemaphoreTake(gpioMutex,pdMS_TO_TICKS(10))){
    *status = (bool)gpio_get_level(pin);
    xSemaphoreGive(gpioMutex);
    return true;
  } else{
    return false;
  }
}

bool setPinState(uint8_t pin, bool new_status){
  if(xSemaphoreTake(gpioMutex,pdMS_TO_TICKS(10))){
    if (gpio_set_level(pin, new_status) == ESP_OK){
      xSemaphoreGive(gpioMutex);
      return true;
    } else{
      xSemaphoreGive(gpioMutex);
      return false;
    }
  } else{
    return false;
  }
}
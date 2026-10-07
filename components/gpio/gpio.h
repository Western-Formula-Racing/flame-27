#pragma once
typedef enum{
  GPIO_BSPD = 0,
  GPIO_CHARGE,
  GPIO_AMS,
  GPIO_LATCH,
  GPIO_HVACT,
  GPIO_AIRN,
  GPIO_IMD,
  GPIO_DRDY,
  GPIO_RTML,
  GPIO_TSSI,
  GPIO_AMSOK,
  GPIO_PRECHOK,
  GPIO_RED,
  GPIO_GREEN,
  GPIO_FANEN,
  GPIO_MAX
} gpioIndex_e;

typedef enum {
  GPIO_DIR_IN,
  GPIO_DIR_OUT,
} gpioDir_e;

// Struct to hold information about each GPIO, doesn't change in runtime.
typedef struct {
  uint8_t pinIndex;
  gpioDir_e dir;
  const char* pinName;
} gpioInfo_t;

extern const gpioInfo_t gpioInfo[GPIO_MAX];

// sets up GPIO
void GPIO_Setup(void);

// configures the boot mode select pin (PIN_MODE_SELECT) with a pull-down and reads it
// @returns true if HIGH, false if LOW
bool readModeSelect(void);

//gets state of digital IO pin
// @param {uint8_t} pin pin to read state of
// @param status pointer to boolean to write status of
// @returns true on success, false on failed read.
bool getPinState(uint8_t pin, bool* status);

// sets the state of a digital IO pin
// @param pin pint to set state of
// @param new_status boolean pin status
// @returns true on success, false on failed set
bool setPinState(uint8_t pin, bool new_status);

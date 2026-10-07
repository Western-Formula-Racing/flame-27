#pragma once

typedef enum {
  STATE_IDLE,
  STATE_PRECHARGE,
  STATE_HV_ACTIVE,
  STATE_CHARGING,
  STATE_BALANCING,
  STATE_BALANCE_COMPLETE,
  STATE_FAULT
} state_e;

void stateTask (void *pvParameters);
state_e getCurrentState();

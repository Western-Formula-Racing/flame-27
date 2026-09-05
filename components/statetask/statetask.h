#pragma once

typedef enum {
  IDLE,
  PRCHARGE,
  HV_ACTIVE,
  CHARGING,
  BALANCING,
  FAULT
} state_e;

void stateTask (void *pvParameters);
state_e getCurrentState();

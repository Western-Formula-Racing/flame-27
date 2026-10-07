#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "coretask.h"

void logTask(void *pvParameters);
void startLoggerTask();

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "measuretask.h"

void logTask(void *pvParameters);
void startLoggerTask();

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"

#include "logger.h"
#include "measuretask.h"
#include "config.h"

static const char* TAG = "logger";

void sendTeleplot(const Data_t* data){
  // format all data in a big block then send it all at once
  //teleplot telemetry format: >[name]:[optional timestamp]:[value]§[unit]|[flags]
  //flags include xy for xy plots, t for text, np to disable auto plot,
  static char json_buf[4096];
  int offset = 0;
  // per module telemetry
  for(int i=0; i<NUM_MODULES; i++){
    //raw cell voltages
    for(int j=0; j<CELLS_PER_MODULE; j++){
      offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">v[%d][%d], Module %d Voltages:%.2f\n",i,j,i,data->ADBMS_cellVoltages[i][j]);
    }
    //filtered cell voltages
    for(int j=0; j<CELLS_PER_MODULE; j++){
      offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">fv[%d][%d], Filtered Module %d Voltages:%.2f\n",i,j,i,data->ADBMS_cellVoltages[i][j]);
    }
    //cell temps
    for(int j=0; j<THERMISTORS_PER_MODULE; j++){
      offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">t[%d][%d], Module %d Temps:%.2f\n",i,j,i,data->ADBMS_temps[i][j]);
    }
    //overall module voltage
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">vpv[%d], Module %d voltage:%.2f\n",i,i,data->ADBMS_moduleVoltage[i]);
    // ground reference
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">vmv[%d], Module %d ground:%.2f|np\n",i,i,data->ADBMS_VMV[i]);
    // internal temp
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">itmp[%d], Module %d ITMP:%.2f|np\n",i,i,data->ADBMS_STAT.itmp[i]);
  }
  if (offset > 0 && offset < sizeof(json_buf)) {
    usb_serial_jtag_write_bytes(json_buf, offset, pdMS_TO_TICKS(100));
  }
}

void logTask(void *pvParameters){
  usb_serial_jtag_driver_config_t usbCfg = {
    .tx_buffer_size = 4096,
    .rx_buffer_size = 1024,
  };
  usb_serial_jtag_driver_install(&usbCfg);
  char stats_buf[512];
  uint8_t logCounter = 0;
  TickType_t last_wake = xTaskGetTickCount();
  while(true){
    logCounter++;
    if(logCounter % 10 == 0){
      // Every 100ms: emit JSON telemetry line over USB serial
      const Data_t* data = getMeasureData();
      sendTeleplot(data);
    }
    if(logCounter % 100 == 0){
      // Every 1s: print runtime stats
      vTaskGetRunTimeStats(stats_buf);
      logCounter = 0;
      ESP_LOGI(TAG, "\n%s", stats_buf);
    }
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
  }
}

void startLoggerTask(){
  xTaskCreatePinnedToCore(
    logTask,
    "loggerTask",
    4096,
    NULL,
    PRIO_LOG,
    NULL,
    0
  );
}
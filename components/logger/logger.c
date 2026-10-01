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

void sendTeleplot(const Data_t* data, const error_t* errors){
  // format all data in a big block then send it all at once
  //teleplot telemetry format: >[name]:[optional timestamp]:[value]§[unit]|[flags]
  //flags include xy for xy plots, t for text, np to disable auto plot
  static char json_buf[4096];
  int offset = 0;

  for(int i=0; i<NUM_MODULES; i++){
    //raw cell voltages
    for(int j=0; j<CELLS_PER_MODULE; j++){
      offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">v[%d][%d], M%dCV:%.4f\n",i,j,i,data->ADBMS_cellVoltages[i][j]);
    }
    //filtered cell voltages
    for(int j=0; j<CELLS_PER_MODULE; j++){
      offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">fv[%d][%d], M%dFCV:%.4f\n",i,j,i,data->ADBMS_filteredCellVoltages[i][j]);
    }
    //cell temps
    for(int j=0; j<THERMISTORS_PER_MODULE; j++){
      offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">t[%d][%d], M%dT:%.4f\n",i,j,i,data->ADBMS_temps[i][j]);
    }
  }
  for(int i=0; i<NUM_MODULES; i++){
    //overall module voltage
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">vpv[%d], M%dV:%.4f\n",i,i,data->ADBMS_moduleVoltage[i]);
  }

  // ground reference
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">Module Ground Reference:");
  for(int i=0; i<NUM_MODULES; i++){
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "vmv[%d]:%.4f,",i,data->ADBMS_VMV[i]);
  }
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "|t,np\n");
  // internal temps
  for(int i=0; i<NUM_MODULES; i++){
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">itmp[%d], M%dITMP:%.4f\n",i,i,data->ADBMS_STAT.itmp[i]);
  }
  // balance states
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">Cells Actively Balancing:");
  for(int i=0; i<NUM_MODULES; i++){
    for(int j=0;j<CELLS_PER_MODULE;j++){
      //add cell if balancing
      if(data->dccMask[i][j]==1){
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "c[%d,%d],",i,j);
      }
    }
  }
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "|t,np\n");
  // errors
  //OV Flags
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">OV Flags: ");
  for(int k = 0; k < NUM_MODULES; k++){
    for (int l = 0; l<CELLS_PER_MODULE;l++){
      if(errors->ADBMS_ovCell[k][l] == 1){
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "c[%d,%d] ",k,l);
      }
    }
  }
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "|t,np\n");
  //UV Flags
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">UV Flags: ");
  for(int k = 0; k < NUM_MODULES; k++){
    for (int l = 0; l<CELLS_PER_MODULE;l++){
      if(errors->ADBMS_uvCell[k][l] == 1){
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "c[%d,%d] ",k,l);
      }
    }
  }
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "|t,np\n");
  //CSFLT Flags
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ">CSFLT Flags: ");
  for(int k = 0; k < NUM_MODULES; k++){
    for (int l = 0; l<CELLS_PER_MODULE;l++){
      if(errors->ADBMS_csFltCell[k][l] == 1){
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "c[%d,%d] ",k,l);
      }
    }
  offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "|t,np\n");
  }
  if (offset > 0 && offset < sizeof(json_buf)) {
    usb_serial_jtag_write_bytes(json_buf, offset, pdMS_TO_TICKS(10));
  } else{
    ESP_LOGE(TAG,"Teleplot Buffer Exceeded!");
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
      // Every 100ms
      const Data_t* data = getMeasureData();
      const error_t* errors = getErrors();
      sendTeleplot(data, errors);
    }
    if(logCounter % 100 == 0){
      // Every 1s
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
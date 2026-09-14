#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CLI_COMMANDS 32
#define MAX_CLI_ARGS     16
#define CLI_BUFFER_SIZE  256

/**
 * @brief Command handler function pointer signature.
 * @param argc Number of arguments (including command name)
 * @param argv Array of argument strings
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*cli_cmd_fn_t)(int argc, char **argv);

/**
 * @brief Command table entry structure.
 */
typedef struct {
    const char *name;
    const char *help;
    cli_cmd_fn_t func;
} cli_cmd_t;

/**
 * @brief Initialize the CLI engine and register default built-in commands.
 * @return ESP_OK on success.
 */
esp_err_t cli_init(void);

/**
 * @brief Register a new command in the CLI.
 * 
 * @param name Command string (e.g. "read_id")
 * @param help Short description / usage string
 * @param func Function to invoke when command is executed
 * @return ESP_OK on success, ESP_ERR_NO_MEM if command table is full.
 */
esp_err_t cli_register_command(const char *name, const char *help, cli_cmd_fn_t func);

/**
 * @brief Process a single raw command string.
 * 
 * @param line Raw input string from serial/console
 */
void cli_process_line(char *line);

/**
 * @brief FreeRTOS task entry point for processing serial CLI input.
 */
void cli_task(void *pvParameters);

/**
 * @brief Start the FreeRTOS CLI task.
 * 
 * @param priority Task priority
 * @param stack_size Stack size in bytes
 */
void cli_start_task(UBaseType_t priority, uint32_t stack_size);

/**
 * @brief Get the currently configured target module number.
 */
uint8_t cli_get_target_module(void);

/**
 * @brief Set the target module number.
 */
void cli_set_target_module(uint8_t module_num);

#ifdef __cplusplus
}
#endif


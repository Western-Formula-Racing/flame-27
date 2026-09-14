#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/usb_serial_jtag.h"

#include "cli_tool.h"
#include "ADBMS6830.h"
#include "statetask.h"
#include "measuretask.h"
#include "config.h"

static const char *TAG = "CLI";

static cli_cmd_t s_cmd_table[MAX_CLI_COMMANDS];
static int s_cmd_count = 0;

esp_err_t cli_register_command(const char *name, const char *help, cli_cmd_fn_t func) {
    if (name == NULL || func == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_cmd_count >= MAX_CLI_COMMANDS) {
        ESP_LOGE(TAG, "Command table full! Cannot register command '%s'", name);
        return ESP_ERR_NO_MEM;
    }

    // Prevent duplicate registrations
    for (int i = 0; i < s_cmd_count; i++) {
        if (strcmp(s_cmd_table[i].name, name) == 0) {
            s_cmd_table[i].help = help;
            s_cmd_table[i].func = func;
            return ESP_OK;
        }
    }

    s_cmd_table[s_cmd_count].name = name;
    s_cmd_table[s_cmd_count].help = help != NULL ? help : "";
    s_cmd_table[s_cmd_count].func = func;
    s_cmd_count++;

    return ESP_OK;
}

// Built-in command handlers
static int cmd_help(int argc, char **argv) {
    printf("\r\n================ FLAME-27 BMS CLI Commands ================\r\n");
    for (int i = 0; i < s_cmd_count; i++) {
        printf("  %-15s : %s\r\n", s_cmd_table[i].name, s_cmd_table[i].help);
    }
    printf("===========================================================\r\n");
    return 0;
}

static int cmd_status(int argc, char **argv) {
    const char *state_str = "UNKNOWN";
    state_e state = getCurrentState();
    switch (state) {
        case IDLE:       state_str = "IDLE"; break;
        case PRCHARGE:   state_str = "PRCHARGE"; break;
        case HV_ACTIVE:  state_str = "HV_ACTIVE"; break;
        case CHARGING:   state_str = "CHARGING"; break;
        case BALANCING:  state_str = "BALANCING"; break;
        case FAULT:      state_str = "FAULT"; break;
    }
    printf("\r\n--- FLAME-27 Firmware Status ---\r\n");
    printf("Daisy-Chained Modules : %d\r\n", NUM_MODULES);
    printf("System State          : %s (%d)\r\n", state_str, state);
    printf("Free Heap             : %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    printf("--------------------------------\r\n");
    return 0;
}

static int cmd_read_id(int argc, char **argv) {
    printf("Reading ADBMS Serial IDs across %d daisy-chained module(s)...\r\n", NUM_MODULES);
    ADBMSReadSerialIDs();
    return 0;
}

static int cmd_reg_dump(int argc, char **argv) {
    printf("Executing ADBMS Serial Register Dump...\r\n");
    ADBMSSerialRegisterDump();
    return 0;
}

static int cmd_read_voltages(int argc, char **argv) {
    static float cell_voltages[NUM_MODULES][CELLS_PER_MODULE];
    printf("Reading cell voltages across %d module(s)...\r\n", NUM_MODULES);
    ADBMSReadFilteredVoltages(cell_voltages, NUM_MODULES, CELLS_PER_MODULE);
    for (int m = 0; m < NUM_MODULES; m++) {
        printf("Module %d Voltages:\r\n  ", m + 1);
        for (int c = 0; c < CELLS_PER_MODULE; c++) {
            printf("C%02d: %.3fV  ", c + 1, cell_voltages[m][c]);
            if ((c + 1) % 4 == 0 && (c + 1) < CELLS_PER_MODULE) {
                printf("\r\n  ");
            }
        }
        printf("\r\n");
    }
    float delta = getMaxCellVoltageDelta(cell_voltages, NUM_MODULES, CELLS_PER_MODULE);
    printf("Max Cell Voltage Delta: %.4f V\r\n", delta);
    return 0;
}

static int cmd_log_level(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: log_level <none|error|warn|info|debug|verbose|0-5>\r\n");
        return 1;
    }

    esp_log_level_t level;
    const char *level_name = argv[1];

    if (strcasecmp(level_name, "none") == 0 || strcmp(level_name, "0") == 0) {
        level = ESP_LOG_NONE;
        level_name = "NONE";
    } else if (strcasecmp(level_name, "error") == 0 || strcmp(level_name, "1") == 0) {
        level = ESP_LOG_ERROR;
        level_name = "ERROR";
    } else if (strcasecmp(level_name, "warn") == 0 || strcasecmp(level_name, "warning") == 0 || strcmp(level_name, "2") == 0) {
        level = ESP_LOG_WARN;
        level_name = "WARN";
    } else if (strcasecmp(level_name, "info") == 0 || strcmp(level_name, "3") == 0) {
        level = ESP_LOG_INFO;
        level_name = "INFO";
    } else if (strcasecmp(level_name, "debug") == 0 || strcmp(level_name, "4") == 0) {
        level = ESP_LOG_DEBUG;
        level_name = "DEBUG";
    } else if (strcasecmp(level_name, "verbose") == 0 || strcmp(level_name, "5") == 0) {
        level = ESP_LOG_VERBOSE;
        level_name = "VERBOSE";
    } else {
        printf("Error: Invalid log level '%s'. Allowed: none, error, warn, info, debug, verbose (or 0-5)\r\n", level_name);
        return 1;
    }

    esp_log_level_set("*", level);
    printf("Global ESP log level set to %s (%d)\r\n", level_name, (int)level);
    return 0;
}

esp_err_t cli_init(void) {
    s_cmd_count = 0;

    cli_register_command("help",          "List all available CLI commands",              cmd_help);
    cli_register_command("?",             "Alias for help",                              cmd_help);
    cli_register_command("status",        "Show current system status",                  cmd_status);
    cli_register_command("read_id",       "Read serial IDs for daisy-chained modules",    cmd_read_id);
    cli_register_command("reg_dump",      "Dump ADBMS register map",                      cmd_reg_dump);
    cli_register_command("read_voltages", "Read cell voltages for daisy-chained modules", cmd_read_voltages);
    cli_register_command("log_level",     "Set global ESP log level (none..verbose/0-5)", cmd_log_level);

    ESP_LOGI(TAG, "CLI engine initialized with %d built-in commands", s_cmd_count);
    return ESP_OK;
}

void cli_process_line(char *line) {
    // Strip leading whitespace
    while (isspace((unsigned char)*line)) line++;
    if (*line == '\0') return;

    // Tokenize line into argc / argv
    int argc = 0;
    char *argv[MAX_CLI_ARGS];
    char *token = strtok(line, " \t\r\n");
    while (token != NULL && argc < MAX_CLI_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    if (argc == 0) return;

    // Search command table
    for (int i = 0; i < s_cmd_count; i++) {
        if (strcmp(argv[0], s_cmd_table[i].name) == 0) {
            int ret = s_cmd_table[i].func(argc, argv);
            if (ret != 0) {
                printf("Command '%s' returned code %d\r\n", argv[0], ret);
            }
            return;
        }
    }

    printf("Unknown command: '%s'. Type 'help' for available commands.\r\n", argv[0]);
}

static void cli_print_prompt(void) {
    printf("\r\nflame27> ");
    fflush(stdout);
}

void cli_task(void *pvParameters) {
    uint8_t rx_char;
    char line_buf[CLI_BUFFER_SIZE];
    size_t buf_idx = 0;

    vTaskDelay(pdMS_TO_TICKS(100)); // Brief initial delay for UART/USB setup
    cli_print_prompt();

    while (1) {
        int len = usb_serial_jtag_read_bytes(&rx_char, 1, pdMS_TO_TICKS(10));
        if (len > 0) {
            if (rx_char == '\r' || rx_char == '\n') {
                printf("\r\n");
                fflush(stdout);
                if (buf_idx > 0) {
                    line_buf[buf_idx] = '\0';
                    cli_process_line(line_buf);
                    buf_idx = 0;
                }
                cli_print_prompt();
            } else if (rx_char == '\b' || rx_char == 0x7F) { // Backspace
                if (buf_idx > 0) {
                    buf_idx--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (isprint(rx_char)) {
                if (buf_idx < CLI_BUFFER_SIZE - 1) {
                    line_buf[buf_idx++] = (char)rx_char;
                    putchar(rx_char);
                    fflush(stdout);
                }
            }
        }
    }
}

void cli_start_task(UBaseType_t priority, uint32_t stack_size) {
    xTaskCreate(
        cli_task,
        "cli_task",
        stack_size,
        NULL,
        priority,
        NULL
    );
}

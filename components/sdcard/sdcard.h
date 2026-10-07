#pragma once
#include <stdbool.h>
#include "sdmmc_cmd.h"

// All functions assume the shared SPI bus (SPI2_HOST) has already been initialised by SPI_Setup().

// mounts the card as a FAT filesystem at SD_MOUNT_POINT so firmware can read/write files.
// @returns true on success
bool sdcard_mount(void);

// unmounts the FAT filesystem and releases the SPI device. Safe to call if not mounted.
void sdcard_unmount(void);

// initialises the card without any filesystem so it can be handed to the USB host as a raw block device.
// the filesystem must NOT be mounted by firmware at the same time.
// @returns card handle, or NULL on failure
sdmmc_card_t* sdcard_open_raw(void);

// @returns true while the FAT filesystem is mounted
bool sdcard_is_mounted(void);

// scans SD_MOUNT_POINT for files named LOGXXXX.csv (X = digit), creates an empty LOG(highest+1).csv
// and remembers its path for sdcard_log_path(). Call once after sdcard_mount(), e.g. at boot.
// @returns true if the file was created, false if the card isn't mounted, all 9999 numbers are used, or creation failed
bool filelog(void);

// @returns full path of the file created by filelog() (e.g. "/sdcard/LOG0007.csv"),
//          or NULL if none has been created since the card was mounted
const char* sdcard_log_path(void);

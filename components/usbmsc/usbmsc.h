#pragma once
#include <stdbool.h>
#include "sdmmc_cmd.h"

// Blocks for USB_HOST_DETECT_MS, then reports whether a USB host is driving the USB port.
// A charger or power bank (no SOF packets) does not count as a host.
// Call this before starting TinyUSB.
bool usbmsc_host_present(void);

// Exposes an initialised card to the USB host as a mass storage drive.
// The card must be raw (see sdcard_open_raw()), NOT mounted by firmware.
// This takes over the USB PHY, so USB Serial/JTAG (logging, console, JTAG) stops working until reset.
// @returns true on success
bool usbmsc_start(sdmmc_card_t* card);
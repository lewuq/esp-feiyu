#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Two DMA buffers: draw into one while the other is in flight. */
esp_err_t display_init(void);
uint8_t *display_buffer(unsigned slot);
esp_err_t display_submit(unsigned slot);
esp_err_t display_set_backlight(bool enabled);

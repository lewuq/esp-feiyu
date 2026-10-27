#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef enum { BUTTON_PREV, BUTTON_NEXT, BUTTON_POWER } button_event_t;
esp_err_t buttons_init(void);
bool buttons_get_event(button_event_t *event);

#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef enum { VOICE_IDLE, VOICE_RECORDING, VOICE_WAITING, VOICE_PLAYING, VOICE_ERROR } voice_state_t;
esp_err_t voice_init(void);
bool voice_start(void);
voice_state_t voice_state(void);

#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct { const uint8_t *pixels; int width; int height; } image_t;
#define CHARACTER_COUNT 8
/* RGB565 big-endian plus alpha byte per pixel. */
const image_t *image_actor(unsigned index);
const image_t *image_weapon(unsigned index);
/* One intensity byte and one stroke-order byte for each 135x240 pixel. */
const uint8_t *image_splash(void);

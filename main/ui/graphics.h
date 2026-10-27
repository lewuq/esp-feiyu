#pragma once
#include <stdint.h>
#include "resources/image_assets.h"

void gfx_pixel(uint8_t *buffer,int x,int y,uint16_t color);
void gfx_disc(uint8_t *buffer,int x,int y,int radius,uint16_t color);
void gfx_image(uint8_t *buffer,const image_t *image,int x,int y);

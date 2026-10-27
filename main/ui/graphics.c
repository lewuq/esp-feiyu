#include "ui/graphics.h"
#include "config/board_config.h"
#include <stddef.h>
void gfx_pixel(uint8_t *buf,int x,int y,uint16_t color) {
    if ((unsigned)x>=DISPLAY_WIDTH || (unsigned)y>=DISPLAY_HEIGHT) return;
    size_t i=((size_t)y*DISPLAY_WIDTH+x)*2;
    buf[i]=color>>8; buf[i+1]=color;
}
void gfx_disc(uint8_t *b,int x,int y,int radius,uint16_t color) {
    for (int dy=-radius;dy<=radius;dy++) for (int dx=-radius;dx<=radius;dx++)
        if (dx*dx+dy*dy<=radius*radius) gfx_pixel(b,x+dx,y+dy,color);
}
void gfx_image(uint8_t *dst,const image_t *image,int x0,int y0) {
    for (int y=0;y<image->height;y++) for (int x=0;x<image->width;x++) {
        int dx=x+x0,dy=y+y0;
        if ((unsigned)dx>=DISPLAY_WIDTH || (unsigned)dy>=DISPLAY_HEIGHT) continue;
        const uint8_t *s=image->pixels+3*(y*image->width+x); unsigned a=s[2];
        if (!a) continue;
        size_t k=((size_t)dy*DISPLAY_WIDTH+dx)*2;
        uint16_t c=(s[0]<<8)|s[1];
        if (a<255) {
            uint16_t d=(dst[k]<<8)|dst[k+1];
            unsigned r=(((c>>11)&31)*a+((d>>11)&31)*(255-a)+127)/255;
            unsigned g=(((c>>5)&63)*a+((d>>5)&63)*(255-a)+127)/255;
            unsigned b=((c&31)*a+(d&31)*(255-a)+127)/255;
            c=(r<<11)|(g<<5)|b;
        }
        dst[k]=c>>8; dst[k+1]=c;
    }
}

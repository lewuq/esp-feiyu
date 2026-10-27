#include "ui/ui.h"
#include "config/board_config.h"
#include "config/demo_config.h"
#include "resources/image_assets.h"
#include "ui/graphics.h"
#include <math.h>
#include <string.h>
#define DEEPSEEK_BLUE 0x4b5f /* RGB565 for #4d6bfe */

void ui_draw_intro(uint8_t *buffer,int64_t elapsed_ms) {
    memset(buffer,0,DISPLAY_WIDTH*DISPLAY_HEIGHT*2);
    /* A linear clock drives the stored stroke order. The 4-step feather softens the pen edge. */
    int64_t progress = elapsed_ms>=INTRO_WRITE_MS ? 255*256+1024 : elapsed_ms*(255*256+1024)/INTRO_WRITE_MS;
    const uint8_t *ink=image_splash();
    for (unsigned i=0;i<DISPLAY_WIDTH*DISPLAY_HEIGHT;i++) {
        unsigned intensity=ink[i*2], order=ink[i*2+1];
        if (!intensity) continue;
        int64_t visibility=progress-(int64_t)order*256;
        if (visibility<=0) continue;
        if (visibility<1024) intensity=(intensity*(unsigned)visibility)/1024;
        unsigned r=(77*intensity+127)/255;
        unsigned g=(107*intensity+127)/255;
        unsigned b=(254*intensity+127)/255;
        uint16_t c=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
        buffer[i*2]=c>>8; buffer[i*2+1]=c;
    }
}
void ui_draw_scene(uint8_t *buffer,unsigned index,int64_t elapsed_ms) {
    memset(buffer,0,DISPLAY_WIDTH*DISPLAY_HEIGHT*2);
    float t=elapsed_ms/1000.0f;
    for (int i=0;i<18;i++) {
        int x=9+(i*47)%117,y=19+(i*71)%195;
        if (sinf(t*1.6f+i)>0.15f) gfx_disc(buffer,x,y,i%7==0?1:0,0x21cf);
    }
    int bob=(int)lroundf(4*sinf(t*2.2f));
    int sway=(int)lroundf(3*sinf(t*1.3f));
    gfx_image(buffer,image_actor(index),11+sway,47+bob);
    gfx_image(buffer,image_weapon(index),69+(int)lroundf(4*sinf(t*2.7f)),142+(int)lroundf(4*cosf(t*2.7f)));
    for (int i=0;i<CHARACTER_COUNT;i++) gfx_disc(buffer,39+i*8,214,i==(int)index?2:1,i==(int)index?DEEPSEEK_BLUE:0x18e9);
}

#include "resources/image_assets.h"
#include <assert.h>
#define ASSET(n) extern const uint8_t n[] asm("_binary_" #n "_bin_start")
ASSET(home); ASSET(beg); ASSET(caught); ASSET(defiant); ASSET(dust); ASSET(flee); ASSET(knocked); ASSET(scared);
ASSET(gatling); ASSET(broom); ASSET(sock); ASSET(rice); ASSET(splash);
static const image_t actors[]={
    {home,112,112},{beg,112,112},{caught,112,112},{defiant,112,112},
    {dust,112,112},{flee,112,112},{knocked,112,112},{scared,112,112}
};
static const image_t weapons[]={
    {rice,60,60},{sock,60,60},{broom,60,60},{gatling,60,60},
    {broom,60,60},{rice,60,60},{gatling,60,60},{sock,60,60}
};
const image_t *image_actor(unsigned index) { assert(index<CHARACTER_COUNT); return &actors[index]; }
const image_t *image_weapon(unsigned index) { assert(index<CHARACTER_COUNT); return &weapons[index]; }
const uint8_t *image_splash(void) { return splash; }

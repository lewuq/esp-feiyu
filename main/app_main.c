#include <stdbool.h>
#include <stdint.h>
#include "config/demo_config.h"
#include "display/display.h"
#include "input/buttons.h"
#include "resources/image_assets.h"
#include "ui/ui.h"
#include "voice/voice.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG="dafeiyu";
void app_main(void) {
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(buttons_init());
    esp_err_t voice_err = voice_init();
    if (voice_err != ESP_OK) ESP_LOGW(TAG, "Voice unavailable: %s", esp_err_to_name(voice_err));
    unsigned index=0,slot=0;
    bool awake=true,first_frame=true;
    int64_t start=esp_timer_get_time()/1000;
    TickType_t last=xTaskGetTickCount();
    ESP_LOGI(TAG,"USR1 previous, USR2 next, USR3 backlight; brightness %d%%",DISPLAY_BRIGHTNESS_PERCENT);
    for (;;) {
        button_event_t event;
        while (buttons_get_event(&event)) {
            if (event==BUTTON_POWER) { awake=!awake; ESP_ERROR_CHECK(display_set_backlight(awake)); }
            else if (awake && event == BUTTON_PREV) index=(index+1)%CHARACTER_COUNT;
            else if (awake && event == BUTTON_NEXT && !voice_start()) ESP_LOGW(TAG, "Voice not ready or busy");
        }
        if (awake) {
            int64_t elapsed=esp_timer_get_time()/1000-start;
            uint8_t *buffer=display_buffer(slot);
            if (elapsed<INTRO_TOTAL_MS) ui_draw_intro(buffer,elapsed);
            else ui_draw_scene(buffer,index,elapsed-INTRO_TOTAL_MS);
            esp_err_t err=display_submit(slot);
            if (err==ESP_OK) {
                if (first_frame) { ESP_ERROR_CHECK(display_set_backlight(true)); first_frame=false; }
                slot^=1;
            } else ESP_LOGE(TAG,"display transfer failed: %s",esp_err_to_name(err));
        }
        vTaskDelayUntil(&last,pdMS_TO_TICKS(FRAME_INTERVAL_MS));
    }
}

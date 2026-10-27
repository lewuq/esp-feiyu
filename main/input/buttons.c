#include "input/buttons.h"
#include "config/board_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static QueueHandle_t queue;
static void button_task(void *arg) {
    (void)arg;
    const int pins[]={PIN_BUTTON_PREV,PIN_BUTTON_NEXT,PIN_BUTTON_POWER};
    int stable[3]={1,1,1}, sample[3]={1,1,1}, count[3]={0};
    for (;;) {
        for (int i=0;i<3;i++) {
            int v=gpio_get_level(pins[i]);
            if (v==sample[i]) { if(count[i]<3) count[i]++; }
            else { sample[i]=v; count[i]=1; }
            if (count[i]>=3 && v!=stable[i]) {
                stable[i]=v;
                if (!v) { button_event_t event=(button_event_t)i; xQueueSend(queue,&event,0); }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
esp_err_t buttons_init(void) {
    const gpio_config_t cfg={.pin_bit_mask=(1ULL<<PIN_BUTTON_PREV)|(1ULL<<PIN_BUTTON_NEXT)|(1ULL<<PIN_BUTTON_POWER),
        .mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
    esp_err_t err=gpio_config(&cfg); if (err!=ESP_OK) return err;
    queue=xQueueCreate(16,sizeof(button_event_t)); if (!queue) return ESP_ERR_NO_MEM;
    return xTaskCreate(button_task,"buttons",2048,NULL,5,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
bool buttons_get_event(button_event_t *event) { return xQueueReceive(queue,event,0)==pdTRUE; }

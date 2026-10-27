#include "display/display.h"
#include <assert.h>
#include "config/board_config.h"
#include "config/demo_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define FRAME_BYTES (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2)
#define BACKLIGHT_TIMER LEDC_TIMER_0
#define BACKLIGHT_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_MAX_DUTY ((1U << 13) - 1)
static SemaphoreHandle_t frame_done;
static esp_lcd_panel_handle_t panel;
static uint8_t *frames[2];

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *ctx) {
    (void)io; (void)event; (void)ctx;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(frame_done, &wake);
    return wake == pdTRUE;
}
esp_err_t display_set_backlight(bool enabled) {
    uint32_t duty = enabled ? ((BACKLIGHT_MAX_DUTY * DISPLAY_BRIGHTNESS_PERCENT + 50U) / 100U) : 0;
    esp_err_t err = ledc_set_duty(BACKLIGHT_MODE, BACKLIGHT_CHANNEL, duty);
    if (err != ESP_OK) return err;
    return ledc_update_duty(BACKLIGHT_MODE, BACKLIGHT_CHANNEL);
}
esp_err_t display_init(void) {
    frame_done=xSemaphoreCreateBinary();
    if (!frame_done) return ESP_ERR_NO_MEM;
    const ledc_timer_config_t timer={.speed_mode=BACKLIGHT_MODE,.timer_num=BACKLIGHT_TIMER,
        .duty_resolution=LEDC_TIMER_13_BIT,.freq_hz=5000,.clk_cfg=LEDC_AUTO_CLK};
    esp_err_t err=ledc_timer_config(&timer); if (err != ESP_OK) return err;
    const ledc_channel_config_t backlight={.gpio_num=PIN_LCD_BL,.speed_mode=BACKLIGHT_MODE,
        .channel=BACKLIGHT_CHANNEL,.timer_sel=BACKLIGHT_TIMER,.duty=0,.hpoint=0};
    err=ledc_channel_config(&backlight); if (err != ESP_OK) return err;
    spi_bus_config_t bus={.sclk_io_num=PIN_LCD_SCLK,.mosi_io_num=PIN_LCD_MOSI,.miso_io_num=-1,
        .quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=FRAME_BYTES};
    err=spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO); if (err != ESP_OK) return err;
    esp_lcd_panel_io_handle_t io;
    const esp_lcd_panel_io_spi_config_t io_cfg={.dc_gpio_num=PIN_LCD_DC,.cs_gpio_num=PIN_LCD_CS,
        .pclk_hz=40*1000*1000,.lcd_cmd_bits=8,.lcd_param_bits=8,.spi_mode=0,
        .trans_queue_depth=1,.on_color_trans_done=on_color_done};
    err=esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&io_cfg,&io); if (err != ESP_OK) return err;
    const esp_lcd_panel_dev_config_t cfg={.reset_gpio_num=PIN_LCD_RST,
        .rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16};
    err=esp_lcd_new_panel_st7789(io,&cfg,&panel); if (err != ESP_OK) return err;
    err=esp_lcd_panel_reset(panel); if (err != ESP_OK) return err;
    err=esp_lcd_panel_init(panel); if (err != ESP_OK) return err;
    err=esp_lcd_panel_invert_color(panel,true); if (err != ESP_OK) return err;
    err=esp_lcd_panel_set_gap(panel,LCD_X_GAP,LCD_Y_GAP); if (err != ESP_OK) return err;
    err=esp_lcd_panel_disp_on_off(panel,true); if (err != ESP_OK) return err;
    for (unsigned i=0;i<2;i++) {
        frames[i]=heap_caps_malloc(FRAME_BYTES,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
        if (!frames[i]) return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(frame_done);
    return ESP_OK;
}
uint8_t *display_buffer(unsigned slot) { assert(slot<2); return frames[slot]; }
esp_err_t display_submit(unsigned slot) {
    assert(slot<2);
    if (xSemaphoreTake(frame_done,pdMS_TO_TICKS(1000))!=pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err=esp_lcd_panel_draw_bitmap(panel,0,0,DISPLAY_WIDTH,DISPLAY_HEIGHT,frames[slot]);
    if (err!=ESP_OK) xSemaphoreGive(frame_done);
    return err;
}

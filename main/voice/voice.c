#include "voice/voice.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "esp_check.h"
#include <string.h>
#include "sdkconfig.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define RATE 16000
#define RECORD_BYTES (RATE * 2 * 3)
#define CONNECTED BIT0
static const char *TAG = "voice";
static EventGroupHandle_t wifi_events;
static QueueHandle_t requests;
static volatile voice_state_t state = VOICE_IDLE;
static i2s_chan_handle_t mic;
static i2s_chan_handle_t speaker;

voice_state_t voice_state(void) { return state; }
bool voice_start(void) {
    if (!requests || state != VOICE_IDLE) return false;
    int start = 1;
    return xQueueSend(requests, &start, 0) == pdTRUE;
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, CONNECTED);
        esp_wifi_connect();
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(wifi_events, CONNECTED);
}

static esp_err_t init_wifi(void) {
    if (!CONFIG_DAFEIYU_WIFI_SSID[0] || !CONFIG_DAFEIYU_GATEWAY_URL[0]) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    if (strncmp(CONFIG_DAFEIYU_GATEWAY_URL, "https://", 8) == 0) {
        esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&sntp), TAG, "time sync init");
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL), TAG, "wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL), TAG, "ip events");
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, CONFIG_DAFEIYU_WIFI_SSID, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, CONFIG_DAFEIYU_WIFI_PASSWORD, sizeof(cfg.sta.password));
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "wifi config");
    return esp_wifi_start();
}

static esp_err_t init_audio(void) {
    i2s_chan_config_t rx = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&rx, NULL, &mic), TAG, "mic channel");
    i2s_pdm_rx_config_t pdm = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = { .clk = GPIO_NUM_1, .din = GPIO_NUM_2 },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_pdm_rx_mode(mic, &pdm), TAG, "mic mode");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(mic), TAG, "mic enable");
    i2s_chan_config_t tx = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&tx, &speaker, NULL), TAG, "speaker channel");
    i2s_std_config_t out = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = { .mclk = I2S_GPIO_UNUSED, .bclk = GPIO_NUM_39, .ws = GPIO_NUM_40,
                      .dout = GPIO_NUM_38, .din = I2S_GPIO_UNUSED },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(speaker, &out), TAG, "speaker mode");
    return i2s_channel_enable(speaker);
}

static esp_err_t record(uint8_t *buf) {
    /* Restart capture to discard audio buffered before the button press. */
    ESP_RETURN_ON_ERROR(i2s_channel_disable(mic), TAG, "mic reset");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(mic), TAG, "mic restart");
    size_t off = 0;
    while (off < RECORD_BYTES) {
        size_t got = 0;
        esp_err_t err = i2s_channel_read(mic, buf + off, RECORD_BYTES - off, &got, pdMS_TO_TICKS(1000));
        if (err != ESP_OK || !got) return err == ESP_OK ? ESP_ERR_TIMEOUT : err;
        off += got;
    }
    return ESP_OK;
}

static esp_err_t play_response(esp_http_client_handle_t client) {
    uint8_t input[2048];
    int16_t output[2048];
    int carry = -1;
    for (;;) {
        int n = esp_http_client_read(client, (char *)input, sizeof(input));
        if (n < 0) return ESP_FAIL;
        if (!n) break;
        int count = 0, i = 0;
        if (carry >= 0) {
            int16_t sample = (int16_t)(carry | (input[i++] << 8));
            output[count++] = sample; output[count++] = sample; carry = -1;
        }
        while (i + 1 < n) {
            int16_t sample = (int16_t)(input[i] | (input[i + 1] << 8)); i += 2;
            output[count++] = sample; output[count++] = sample;
        }
        if (i < n) carry = input[i];
        size_t off = 0, total = count * sizeof(int16_t);
        while (off < total) {
            size_t written = 0;
            esp_err_t err = i2s_channel_write(speaker, (uint8_t *)output + off, total - off, &written, pdMS_TO_TICKS(2000));
            if (err != ESP_OK || !written) return err == ESP_OK ? ESP_ERR_TIMEOUT : err;
            off += written;
        }
    }
    return carry < 0 ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t exchange(const uint8_t *pcm) {
    if (strncmp(CONFIG_DAFEIYU_GATEWAY_URL, "https://", 8) == 0 && time(NULL) < 1704067200) {
        ESP_RETURN_ON_ERROR(esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)), TAG, "time sync required for HTTPS");
    }
    esp_http_client_config_t cfg = { .url = CONFIG_DAFEIYU_GATEWAY_URL,
        .timeout_ms = 90000, .crt_bundle_attach = esp_crt_bundle_attach };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_FAIL;
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        char id[13];
        snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        esp_http_client_set_header(client, "X-Device-ID", id);
    }
    if (CONFIG_DAFEIYU_GATEWAY_TOKEN[0]) {
        char auth[sizeof(CONFIG_DAFEIYU_GATEWAY_TOKEN) + 8];
        snprintf(auth, sizeof(auth), "Bearer %s", CONFIG_DAFEIYU_GATEWAY_TOKEN);
        esp_http_client_set_header(client, "Authorization", auth);
    }
    if (esp_http_client_open(client, RECORD_BYTES) != ESP_OK) goto done;
    int offset = 0;
    while (offset < RECORD_BYTES) {
        int n = esp_http_client_write(client, (const char *)pcm + offset, RECORD_BYTES - offset);
        if (n <= 0) goto done;
        offset += n;
    }
    if (esp_http_client_fetch_headers(client) < 0 || esp_http_client_get_status_code(client) != 200) goto done;
    state = VOICE_PLAYING;
    err = play_response(client);
done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

static void voice_task(void *arg) {
    (void)arg;
    int start;
    while (xQueueReceive(requests, &start, portMAX_DELAY)) {
        if (!(xEventGroupGetBits(wifi_events) & CONNECTED)) { state = VOICE_ERROR; vTaskDelay(pdMS_TO_TICKS(1500)); state = VOICE_IDLE; continue; }
        uint8_t *buffer = heap_caps_malloc(RECORD_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buffer) { state = VOICE_ERROR; vTaskDelay(pdMS_TO_TICKS(1500)); state = VOICE_IDLE; continue; }
        state = VOICE_RECORDING;
        esp_err_t err = record(buffer);
        if (err == ESP_OK) { state = VOICE_WAITING; err = exchange(buffer); }
        free(buffer);
        if (err != ESP_OK) { ESP_LOGE(TAG, "voice turn failed: %s", esp_err_to_name(err)); state = VOICE_ERROR; vTaskDelay(pdMS_TO_TICKS(1500)); }
        state = VOICE_IDLE;
    }
}

esp_err_t voice_init(void) {
    wifi_events = xEventGroupCreate();
    requests = xQueueCreate(1, sizeof(int));
    if (!wifi_events || !requests) return ESP_ERR_NO_MEM;
    esp_err_t err = init_audio();
    if (err == ESP_OK) err = init_wifi();
    if (err == ESP_OK && xTaskCreate(voice_task, "voice", 8192, NULL, 5, NULL) != pdPASS) err = ESP_ERR_NO_MEM;
    if (err != ESP_OK) state = VOICE_ERROR;
    return err;
}

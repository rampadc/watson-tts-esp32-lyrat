/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "audio_pipeline.h"
#include "board.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"

#include "watson_tts.h"

static const char *TAG = "WATSON_TTS";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    tcpip_adapter_init();

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("WATSON_TTS", ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] Start and wait for Wi-Fi");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    err = periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
    ESP_LOGI(TAG, "[ 1 ] Wi-Fi connected: %d", err == ESP_OK);

    ESP_LOGI(TAG, "[ 2 ] Start audio codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Set up  TTS");
    watson_tts_handle_t tts = watson_tts_init();
    if (tts != NULL) {
        ESP_LOGI(TAG, "[3.1] TTS set up correctly");
    } else {
        ESP_LOGE(TAG, "[3.1] ERROR: Watson TTS setup failed");
        esp_cpu_reset(1);
    }

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from the pipeline");
    watson_tts_set_listener(tts, evt);

    watson_tts_synthesize(tts, 
        "All dogs go to heaven.");

    while(1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, portMAX_DELAY) != ESP_OK) {
            ESP_LOGW(TAG, "[ * ] Event process failed: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                     msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);
            continue;
        }

        ESP_LOGI(TAG, "[ * ] Event received: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
            msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);

        
        watson_handle_msg(tts, msg);
    }

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    watson_tts_destroy(tts);
    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);
    esp_periph_set_destroy(set);
}

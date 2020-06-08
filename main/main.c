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

static const char *TAG = "WATSON_STT_TTS";

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
    ESP_LOGI(TAG, "[1.1] Wi-Fi connected: %d", err == ESP_OK);


    for (int i = 10; i >= 0; i--) {
        ESP_LOGI(TAG, "[ 3 ] Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "[ 4 ] Restarting now");
    fflush(stdout);
    esp_restart();
}

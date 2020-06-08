#include "esp_log.h"
#include "audio_pipeline.h"

#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"

#include "watson_tts.h"
#include "sdkconfig.h"

#include <string.h>

typedef struct watson_tts {
  audio_pipeline_handle_t pipeline;
  audio_element_handle_t  http_stream_reader;
  audio_element_handle_t  i2s_stream_writer;
  audio_element_handle_t  mp3_decoder;
  char                    *url_buffer;

  char                    *endpoint_url;
  char                    *voice;
} watson_tts_t;

static const char *TAG = "WATSON_TTS";

#define WATSON_TTS_TEMPLATE "%s/v1/synthesize?text=%s&accept=audio/mp3&voice=%s"

watson_tts_handle_t watson_tts_init() {
  watson_tts_t *tts = calloc(1, sizeof(watson_tts_t));
  AUDIO_MEM_CHECK(TAG, tts, return NULL);

  ESP_LOGI(TAG, "[TTS.0] Filling in config for Watson TTS");
  tts->url_buffer = malloc(DEFAULT_TTS_BUFFER_SIZE);
  AUDIO_MEM_CHECK(TAG, tts->url_buffer, goto exit_tts_init);
  // tts->api_key = CONFIG_TTS_APIKEY;
  // AUDIO_MEM_CHECK(TAG, tts->api_key, goto exit_tts_init);
  tts->endpoint_url = CONFIG_TTS_URL_WITH_APIKEY;
  AUDIO_MEM_CHECK(TAG, tts->endpoint_url, goto exit_tts_init);
  tts->voice = CONFIG_TTS_VOICE;
  AUDIO_MEM_CHECK(TAG, tts->voice, goto exit_tts_init);
  ESP_LOGI(TAG, "[TTS.0] Endpoint URL: %s", tts->endpoint_url);
  ESP_LOGI(TAG, "[TTS.0] Voice: %s", tts->voice);
  
  ESP_LOGI(TAG, "[TTS.0] Creating an audio pipeline for Watson TTS");
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  tts->pipeline = audio_pipeline_init(&pipeline_cfg);

  ESP_LOGD(TAG, "[TTS.0] Assuming audio codec is already configured...");

  ESP_LOGI(TAG, "[TTS.1] Create HTTP stream to read data");
  http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
  tts->http_stream_reader = http_stream_init(&http_cfg);

  ESP_LOGI(TAG, "[TTS.2] Create I2S stream to write data to codec chip");
  i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
  i2s_cfg.type = AUDIO_STREAM_WRITER;
  tts->i2s_stream_writer = i2s_stream_init(&i2s_cfg);

  ESP_LOGI(TAG, "[TTS.3] Create MP3 decoder to decode mp3 file");
  mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
  tts->mp3_decoder = mp3_decoder_init(&mp3_cfg);

  ESP_LOGI(TAG, "[TTS.4] Register all elements to the audio pipeline");
  audio_pipeline_register(tts->pipeline, tts->http_stream_reader, "tts_http");
  audio_pipeline_register(tts->pipeline, tts->mp3_decoder, "tts_mp3");
  audio_pipeline_register(tts->pipeline, tts->i2s_stream_writer, "tts_i2s");

  ESP_LOGI(TAG, "[TTS.5] Link elements: http_stream->mp3_decoder->i2s_stream->codec");
  audio_pipeline_link(tts->pipeline, (const char *[]) {"tts_http", "tts_mp3", "tts_i2s"}, 3);

  ESP_LOGI(TAG, "[TTS.6] Watson TTS setup completed, returning TTS handle");
  return tts;
exit_tts_init:
  ESP_LOGE(TAG, "[TTS.E] Not enough memory for TTS");
  return NULL;
}

esp_err_t watson_tts_synthesize(watson_tts_handle_t tts, const char *text) {
  // Create URL from strings
  // WATSON_TTS_TEMPLATE "%s/v1/synthesize?text=%s&accept=audio/mp3&voice=%s"

  char *dupText = strdup(text);
  if (dupText == NULL) {
    ESP_LOGE(TAG, "ERROR: No memory available");
    return ESP_ERR_NO_MEM;
  }
  snprintf(tts->url_buffer, DEFAULT_TTS_BUFFER_SIZE, 
    WATSON_TTS_TEMPLATE, tts->endpoint_url, dupText, tts->voice);
  ESP_LOGI(TAG, "[TTS.S] GET %s", tts->url_buffer);
  ESP_LOGI(TAG, "%s", tts->url_buffer);

  audio_pipeline_reset_items_state(tts->pipeline);
  audio_pipeline_reset_ringbuffer(tts->pipeline);
  audio_element_set_uri(tts->http_stream_reader, tts->url_buffer);
  audio_pipeline_run(tts->pipeline);
  return ESP_OK;
}

esp_err_t watson_tts_destroy(watson_tts_handle_t tts) {
  if (tts == NULL) {
    return ESP_FAIL;
  }

  audio_pipeline_terminate(tts->pipeline);
  audio_pipeline_remove_listener(tts->pipeline);
  audio_pipeline_deinit(tts->pipeline);
  audio_element_deinit(tts->i2s_stream_writer);
  audio_element_deinit(tts->mp3_decoder);
  audio_element_deinit(tts->http_stream_reader);
  free(tts->url_buffer);
  free(tts);
  return ESP_OK;
}

esp_err_t watson_tts_set_listener(watson_tts_handle_t tts, audio_event_iface_handle_t listener) {
  if (listener) {
      audio_pipeline_set_listener(tts->pipeline, listener);
  }
  return ESP_OK;
}

void watson_handle_msg(watson_tts_handle_t tts, audio_event_iface_msg_t msg) {
  if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
    && msg.source == (void *) tts->mp3_decoder
    && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
    audio_element_info_t music_info = {0};
    audio_element_getinfo(tts->mp3_decoder, &music_info);

    ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
              music_info.sample_rates, music_info.bits, music_info.channels);

    audio_element_setinfo(tts->i2s_stream_writer, &music_info);
    i2s_stream_set_clk(tts->i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
  }

  if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) tts->i2s_stream_writer
    && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
    && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
    ESP_LOGW(TAG, "[ * ] Stop event received");
  }
}
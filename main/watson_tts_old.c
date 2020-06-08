#include "watson_tts.h"
#include "audio_pipeline.h"
#include "esp_http_client.h"
#include "audio_error.h"

#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"

static const char *TAG = "WATSON_TTS";

// Insert api_key, region into endpoint
#define WATSON_TTS_ENDPOINT "https://apikey:%s@api.%s.text-to-speech.watson.cloud.ibm.com"
#define WATSON_TTS_TASK_STACK (8*1024)

typedef struct watson_tts {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t  i2s_writer;
    audio_element_handle_t  http_stream_reader;
    audio_element_handle_t  mp3_decoder;
    char                    *api_key;
    char                    *text;
    char                    *region;
    char                    *buffer;
    char                    *text;
    bool                    is_begin;
    int                     tts_total_read;
    int                     sample_rate;
    int                     remain_len;
    int                     buffer_size;
} watson_tts_t;

static esp_err_t _http_stream_reader_event_handle(http_stream_event_msg_t *msg) {
  esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
  watson_tts_t *tts = (watson_tts_t *)msg->user_data;

  int read_len = 0;

  if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
      // Post text data
      ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
      tts->tts_total_read = 0;
      tts->is_begin = true;
      int payload_len = snprintf(tts->buffer, tts->buffer_size, GOOGLE_TTS_TEMPLATE, tts->sample_rate, tts->lang_code, tts->text);
      esp_http_client_set_post_field(http, tts->buffer, payload_len);
      esp_http_client_set_method(http, HTTP_METHOD_POST);
      esp_http_client_set_header(http, "Content-Type", "application/json");
      tts->remain_len = 0;
      return ESP_OK;
  }

  if (msg->event_id == HTTP_STREAM_ON_RESPONSE) {
      ESP_LOGD(TAG, "[ + ] HTTP client HTTP_STREAM_ON_RESPONSE, lenght=%d", msg->buffer_len);
      /* Read first chunk */
      int process_data_len = 0;
      char *data_ptr = tts->buffer;
      int align_read = msg->buffer_len * 4 / 3;
      align_read -= (align_read % 4);
      align_read -= tts->remain_len;
      read_len = esp_http_client_read(http, tts->remain_len + tts->buffer, align_read);

      // ESP_LOGI(TAG, "Need read = %d, read_len=%d, tts->remain_len=%d", align_read, read_len, tts->remain_len);
      if (read_len <= 0) {
          return read_len;
      }
      process_data_len = read_len + tts->remain_len;

      if (tts->is_begin) {
          tts->is_begin = false;

          while (*data_ptr != ':' && process_data_len) {
              process_data_len--;
              data_ptr++;
          }
          while (*data_ptr != '"' && process_data_len) {
              process_data_len--;
              data_ptr++;
          }
          // *data_ptr = 0;
          data_ptr++;
          process_data_len--;

      }
      unsigned int mp3_len = 0;
      int keep_next_time = 0;
      if (process_data_len > 0) {
          data_ptr[process_data_len] = 0;
          keep_next_time = process_data_len % 4;
          process_data_len -= keep_next_time;
          tts->remain_len = keep_next_time;
          mbedtls_base64_decode(msg->buffer, msg->buffer_len, &mp3_len, (unsigned char *)data_ptr, process_data_len);
          memcpy(tts->buffer, data_ptr + process_data_len, keep_next_time);
      }
      if (mp3_len == 0) {
          return ESP_FAIL;
      }
      return mp3_len;
  }

  if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
    ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
  }

  if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {}

  return ESP_OK;
}

watson_tts_handle_t watson_tts_init(watson_tts_config_t *config) {
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

  watson_tts_t *tts = calloc(1, sizeof(watson_tts_t));
  AUDIO_MEM_CHECK(TAG, tts, return NULL);

  tts->pipeline = audio_pipeline_init(&pipeline_cfg);

  tts->api_key = strdup(config->api_key);
  AUDIO_MEM_CHECK(TAG, tts->api_key, goto exit_tts_init);

  i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
  i2s_cfg.type = AUDIO_STREAM_WRITER;
  tts->i2s_writer = i2s_stream_init(&i2s_cfg);

  mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
  tts->mp3_decoder = mp3_decoder_init(&mp3_cfg);

  audio_pipeline_register(tts->pipeline, tts->http_stream_reader, "tts_http");
  audio_pipeline_register(tts->pipeline, tts->mp3_decoder,        "tts_mp3");
  audio_pipeline_register(tts->pipeline, tts->i2s_writer,         "tts_i2s");
  audio_pipeline_link(tts->pipeline, (const char *[]) {"tts_http", "tts_mp3", "tts_i2s"}, 3);

  // set i2s clock in handler
  // i2s_stream_set_clk(tts->i2s_writer, config->playback_sample_rate, 16, 1);
  return tts;
exit_tts_init:
  watson_tts_destroy(tts);
  return NULL;
}

esp_err_t watson_tts_start(watson_tts_handle_t tts, const char *text) {
  audio_element_set_uri(tts->http_stream_reader, "https://apikey:rRwToyWhAPJe6gtLILX7Mfwx6i4xy7i2A8f4YxB8mq7-@api.au-syd.text-to-speech.watson.cloud.ibm.com/v1/synthesize?voice=en-GB_KateV3Voice&text=The following Watson SDKs are supported by IBM&accept=audio/mp3");
}
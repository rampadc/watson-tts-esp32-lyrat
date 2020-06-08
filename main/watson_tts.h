#ifndef _WATSON_TTS_H
#define _WATSON_TTS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TTS_BUFFER_SIZE (16 * 1024)
typedef struct watson_tts* watson_tts_handle_t;

watson_tts_handle_t watson_tts_init();
esp_err_t watson_tts_synthesize(watson_tts_handle_t, const char *);
esp_err_t watson_tts_set_listener(watson_tts_handle_t, audio_event_iface_handle_t);
void watson_handle_msg(watson_tts_handle_t tts, audio_event_iface_msg_t msg);

#ifdef __cplusplus
}
#endif

#endif
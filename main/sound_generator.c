#include "bsp/esp-bsp.h" // Provides bsp_audio_codec_speaker_init and esp_codec_dev.h
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "robot_config.h"
#include <math.h>

// --- CONFIG ---
#define SAMPLE_RATE 22050 // Must match BSP default for simplicity
#define AMPLITUDE 10000   // Louder (max 32767)
#define PI 3.14159265

typedef enum {
  SOUND_EVENT_NONE,
  SOUND_EVENT_CURIOUS,
  SOUND_EVENT_HAPPY_LOOP // For petting mode
} sound_event_t;

static const char *TAG = "ROBO_VOICE";
static QueueHandle_t sound_queue = NULL;
static volatile bool is_petting_mode = false;
static esp_codec_dev_handle_t spk_codec_dev = NULL;

// --- HELPER: PLAY TONE ---
static void play_tone_internal(int start_freq, int end_freq, int duration_ms,
                               int type) {
  if (!spk_codec_dev)
    return;

  int num_samples = (SAMPLE_RATE * duration_ms) / 1000;

  // Safety limit to avoid huge malloc
  if (num_samples > 44100)
    num_samples = 44100;

  int16_t *samples = malloc(num_samples * sizeof(int16_t));
  if (!samples) {
    ESP_LOGE(TAG, "OOM");
    return;
  }

  for (int i = 0; i < num_samples; i++) {
    float t = (float)i / num_samples;
    float current_freq = start_freq + (end_freq - start_freq) * t;
    float phase = 2.0f * PI * current_freq * (float)i / SAMPLE_RATE;
    float value = 0;

    if (type == 0)
      value = sinf(phase); // Sine
    else if (type == 1)
      value = (sinf(phase) > 0) ? 1.0f : -1.0f; // Square
    else if (type == 2)
      value = 2.0f *
              (phase / (2.0f * PI) - floorf(0.5f + phase / (2.0f * PI))); // Saw

    samples[i] = (int16_t)(value * AMPLITUDE);
  }

  // Blocking write
  esp_codec_dev_write(spk_codec_dev, samples, num_samples * sizeof(int16_t));
  free(samples);
}

// --- MAIN SOUND TASK ---
void sound_server_task(void *arg) {
  sound_event_t evt;

  while (1) {
    // Priority 1: Check if we are being petted (high priority override)
    if (is_petting_mode) {
      // THE HAPPY LOOP (Robot Purr)
      play_tone_internal(400, 600, 100, 0);
      play_tone_internal(600, 400, 100, 0);
      vTaskDelay(pdMS_TO_TICKS(150));

      // Clear queue so we don't build up events while petting
      while (xQueueReceive(sound_queue, &evt, 0))
        ;
    } else {
      // Priority 2: Wait for events
      if (xQueueReceive(sound_queue, &evt, pdMS_TO_TICKS(100)) == pdTRUE) {
        switch (evt) {
        case SOUND_EVENT_CURIOUS:
          play_tone_internal(300, 600, 150, 0);
          break;
        default:
          break;
        }
      }
    }
  }
}

// --- PUBLIC FUNCTIONS ---

void sound_init(void) {
  sound_queue = xQueueCreate(5, sizeof(sound_event_t));

  // Initialize the speaker codec via BSP
  // This handles I2C, I2S, and ES8311 config
  spk_codec_dev = bsp_audio_codec_speaker_init();

  if (spk_codec_dev) {
    ESP_LOGI(TAG, "Audio Codec Initialized (ES8311)");
    esp_codec_dev_set_out_vol(spk_codec_dev, 80); // 0-100
    esp_codec_dev_open(
        spk_codec_dev,
        NULL); // Depending on driver this might be needed or handled in write

    // Start the background task
    xTaskCreate(sound_server_task, "SoundTask", 4096, NULL, 5, NULL);
  } else {
    ESP_LOGE(TAG, "Failed to initialize Audio Codec");
  }
}

// Set the flag for the loop
void sound_set_petting_state(bool is_petting) { is_petting_mode = is_petting; }

// Non-blocking fire-and-forget
void sound_curious(void) {
  sound_event_t evt = SOUND_EVENT_CURIOUS;
  if (sound_queue) {
    xQueueSend(sound_queue, &evt, 0);
  }
}
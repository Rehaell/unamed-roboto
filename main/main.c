#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_random.h" // For esp_random()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdlib.h> // For rand(), srand()

// Include our new headers
#include "network_manager.h"
#include "robot_config.h"
#include "ui_face.h"

static bool is_sleeping = false;

// The "Brain" Loop
static void behavior_timer_cb(lv_timer_t *timer) {
  // 1. Check Sleep
  bool night = network_is_night_time();
  // ... (sleep logic) ...

  if (is_sleeping)
    return;

  // 2. CHECK IF BEING RUBBED - ADD THIS LINE
  if (ui_face_is_rubbing())
    return;

  // 3. Random Behavior
  int dice = rand() % 100;
  if (dice < 5) {
    ui_face_trigger_blink();
  } else if (dice == 50) {
    sound_curious(); // Unleash the beep!
    ui_face_trigger_curious(rand() % 2);
  }
}

void app_main(void) {
  // 1. Hardware Init
  nvs_flash_init();
  network_init();
  sound_init();

  // Seed the random number generator
  srand(esp_random());

  // 2. Display Init
  lv_display_t *disp = bsp_display_start();
  if (disp)
    bsp_display_brightness_set(45);

  // 3. UI Init
  bsp_display_lock(0);
  ui_face_init();
  bsp_display_unlock();

  // 4. Start Brain
  lv_timer_create(behavior_timer_cb, 100, NULL);

  // 5. Start Ears (Later)
  // xTaskCreate(audio_task, "Audio", 4096, NULL, 5, NULL);
}
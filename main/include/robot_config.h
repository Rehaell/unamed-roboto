#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

#include "lvgl.h"

// --- PINS ---
#define I2S_BCK_IO 41
#define I2S_WS_IO 42
#define I2S_DI_IO 40
#define I2S_DO_IO 43
// Microphone settings
#define MIC_SAMPLE_RATE 16000
#define MIC_THRESHOLD_FEAR 8000
#define MIC_THRESHOLD_CURIOUS 2000

// --- WIFI & TIME ---
#define TIME_ZONE_UK "GMT0BST,M3.5.0/1,M10.5.0"
#define SLEEP_HOUR_START 22
#define SLEEP_HOUR_END 7

// --- EYES GEOMETRY ---
#define EYE_W_NORMAL 120
#define EYE_W_WIDE 145
#define EYE_W_SLEEP 4
#define EYE_H_THICKNESS 100
#define EYE_RADIUS_NORMAL 40
#define EYE_RADIUS_HAPPY 50
#define EYE_OFFSET_Y_BASE 70
#define GAZE_SHIFT_AMT 25
#define DIRECTION_UP 1

// --- COLORS ---
#define COLOR_NORMAL lv_color_make(0, 255, 255)  // Cyan
#define COLOR_HAPPY lv_color_make(255, 105, 180) // Hot Pink
#define COLOR_SLEEP lv_color_make(50, 50, 50)    // Grey
#define COLOR_BG lv_color_make(0, 0, 0)          // Black

// Sound Functions
void sound_init(void);
void sound_curious(void);
void sound_fear(void);
void sound_happy(void);
void sound_set_petting_state(bool is_petting);

#endif
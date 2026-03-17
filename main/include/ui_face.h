#ifndef UI_FACE_H
#define UI_FACE_H

#include <stdbool.h>
#include <stdint.h>

static int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max);

static void anim_blink_cb(void * var, int32_t v);

static void anim_curious_cb(void * var, int32_t v);

void ui_face_init(void);

void ui_face_trigger_blink(void);

void ui_face_trigger_curious(int side);

void ui_face_set_sleep(bool sleep);

bool ui_face_is_rubbing(void);

#endif
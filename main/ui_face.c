#include "ui_face.h"
#include "robot_config.h"
#include "lvgl.h" // Need full LVGL access
#include <math.h>
#include <stdlib.h>

static lv_obj_t *left_eye;
static lv_obj_t *right_eye;
static lv_obj_t *left_lid;
static lv_obj_t *right_lid;

static bool is_rubbing = false; // Track touch state

// --- INTERNAL HELPERS ---
static int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// --- ANIMATION CALLBACKS ---
static void anim_blink_cb(void * var, int32_t v) {
    lv_obj_set_width((lv_obj_t *)var, v);
    int32_t offset = (var == left_eye) ? -EYE_OFFSET_Y_BASE : EYE_OFFSET_Y_BASE;
    lv_obj_align((lv_obj_t *)var, LV_ALIGN_CENTER, 0, offset); 
}

static void anim_curious_cb(void * var, int32_t v) {
    int side = (int)var; 
    int32_t l_target_w = (side == 0) ? EYE_W_WIDE : EYE_W_NORMAL;
    int32_t r_target_w = (side == 1) ? EYE_W_WIDE : EYE_W_NORMAL;

    int32_t l_w = map(v, 0, 256, EYE_W_NORMAL, l_target_w);
    int32_t r_w = map(v, 0, 256, EYE_W_NORMAL, r_target_w);

    int32_t l_shift = ((l_w - EYE_W_NORMAL) / 2) * DIRECTION_UP; 
    int32_t r_shift = ((r_w - EYE_W_NORMAL) / 2) * DIRECTION_UP;
    
    int32_t gaze = map(v, 0, 256, 0, (side == 0 ? -GAZE_SHIFT_AMT : GAZE_SHIFT_AMT));

    lv_obj_set_width(left_eye, l_w);
    lv_obj_align(left_eye, LV_ALIGN_CENTER, l_shift, -EYE_OFFSET_Y_BASE + gaze);

    lv_obj_set_width(right_eye, r_w);
    lv_obj_align(right_eye, LV_ALIGN_CENTER, r_shift, EYE_OFFSET_Y_BASE + gaze);
}

// --- HAPPY STATE LOGIC ---
static void set_face_happy(bool happy) {
    lv_anim_del_all(); // Stop any blinking/curious moves immediately

    int32_t target_w = happy ? EYE_H_THICKNESS : EYE_W_NORMAL; 
    int32_t target_radius = happy ? EYE_RADIUS_HAPPY : EYE_RADIUS_NORMAL;
    lv_color_t target_color = happy ? COLOR_HAPPY : COLOR_NORMAL;
    
    // In happy mode, lids close slightly to look like ^_^
    int32_t lid_w = happy ? (EYE_H_THICKNESS / 2) : 0;

    // LEFT
    lv_obj_set_width(left_eye, target_w); 
    lv_obj_set_style_radius(left_eye, target_radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(left_eye, target_color, LV_PART_MAIN);
    lv_obj_set_width(left_lid, lid_w);
    lv_obj_align(left_lid, LV_ALIGN_LEFT_MID, 0, 0); 

    // RIGHT
    lv_obj_set_width(right_eye, target_w);
    lv_obj_set_style_radius(right_eye, target_radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(right_eye, target_color, LV_PART_MAIN);
    lv_obj_set_width(right_lid, lid_w);
    lv_obj_align(right_lid, LV_ALIGN_LEFT_MID, 0, 0);

    // Reset positions to center
    lv_obj_align(left_eye, LV_ALIGN_CENTER, 0, -EYE_OFFSET_Y_BASE);
    lv_obj_align(right_eye, LV_ALIGN_CENTER, 0, EYE_OFFSET_Y_BASE);
}

// --- TOUCH EVENT HANDLER ---
static void screen_touch_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        is_rubbing = true;
        set_face_happy(true);
        sound_set_petting_state(true); // <--- START SOUND LOOP
    } 
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        is_rubbing = false;
        set_face_happy(false);
        sound_set_petting_state(false); // <--- STOP SOUND LOOP
    }
}

// --- PUBLIC FUNCTIONS ---

void ui_face_init(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    
    // Register the touch event on the whole screen
    lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_ALL, NULL);

    // Helper to create an eye
    void create_eye(lv_obj_t **eye, lv_obj_t **lid, int32_t offset) {
        *eye = lv_obj_create(scr);
        lv_obj_remove_style_all(*eye);
        lv_obj_set_size(*eye, EYE_W_NORMAL, EYE_H_THICKNESS);
        lv_obj_set_style_bg_color(*eye, COLOR_NORMAL, 0);
        lv_obj_set_style_bg_opa(*eye, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(*eye, EYE_RADIUS_NORMAL, 0);
        lv_obj_align(*eye, LV_ALIGN_CENTER, 0, offset);
        lv_obj_clear_flag(*eye, LV_OBJ_FLAG_CLICKABLE);

        *lid = lv_obj_create(*eye);
        lv_obj_remove_style_all(*lid);
        lv_obj_set_size(*lid, 0, EYE_H_THICKNESS); 
        lv_obj_set_style_bg_color(*lid, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(*lid, LV_OPA_COVER, 0);
        lv_obj_align(*lid, LV_ALIGN_LEFT_MID, 0, 0); 
    }

    create_eye(&left_eye, &left_lid, -EYE_OFFSET_Y_BASE);
    create_eye(&right_eye, &right_lid, EYE_OFFSET_Y_BASE);

    // 2. Create Touch Layer (Input Layer)
    // ------------------------------------------------
    // This sits ON TOP of everything (because it's created last).
    // It is invisible but clickable.
    lv_obj_t *touch_layer = lv_obj_create(scr);
    
    // Make it cover the entire screen
    lv_obj_set_size(touch_layer, LV_PCT(100), LV_PCT(100));
    
    // Make it invisible
    lv_obj_set_style_bg_opa(touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_layer, 0, 0);
    
    // Make sure it IS clickable
    lv_obj_add_flag(touch_layer, LV_OBJ_FLAG_CLICKABLE);
    
    // Attach the event to THIS layer
    lv_obj_add_event_cb(touch_layer, screen_touch_cb, LV_EVENT_ALL, NULL);
}

void ui_face_trigger_blink(void) {
    if (is_rubbing) return; // Don't blink if being petted
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, left_eye);
    lv_anim_set_values(&a, EYE_W_NORMAL, 5); 
    lv_anim_set_time(&a, 150);
    lv_anim_set_playback_time(&a, 150);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_exec_cb(&a, anim_blink_cb);
    lv_anim_start(&a);

    lv_anim_set_var(&a, right_eye);
    lv_anim_start(&a);
}

void ui_face_trigger_curious(int side) {
    if (is_rubbing) return; // Don't look around if being petted
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, (void*)side);
    lv_anim_set_values(&a, 0, 256);     
    lv_anim_set_time(&a, 200);            
    lv_anim_set_playback_delay(&a, 2000); 
    lv_anim_set_playback_time(&a, 200);   
    lv_anim_set_exec_cb(&a, anim_curious_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

void ui_face_set_sleep(bool sleep) {
    lv_anim_del_all();
    if(sleep) {
        lv_obj_set_width(left_eye, EYE_W_SLEEP);
        lv_obj_set_width(right_eye, EYE_W_SLEEP);
        lv_obj_set_style_bg_color(left_eye, COLOR_SLEEP, 0);
        lv_obj_set_style_bg_color(right_eye, COLOR_SLEEP, 0);
    } else {
        lv_obj_set_width(left_eye, EYE_W_NORMAL);
        lv_obj_set_width(right_eye, EYE_W_NORMAL);
        lv_obj_set_style_bg_color(left_eye, COLOR_NORMAL, 0);
        lv_obj_set_style_bg_color(right_eye, COLOR_NORMAL, 0);
    }
}

// Public getter
bool ui_face_is_rubbing(void) {
    return is_rubbing;
}
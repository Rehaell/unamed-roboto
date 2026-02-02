#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#include "secrets.h"

static const char *TAG = "ROBOTO_RTC";

#define TIME_ZONE      "GMT0BST,M3.5.0/1,M10.5.0" // Example: Central Europe. Google "TZ string" for yours.

// Sleep Schedule (24h format)
#define SLEEP_HOUR_START 00  // 10 PM
#define SLEEP_HOUR_END   5   // 7 AM

// --- EYE CONFIGURATION ---
#define EYE_W_NORMAL      120  
#define EYE_W_WIDE        145  
#define EYE_H_THICKNESS   100  
#define EYE_W_SLEEP       4    // Very thin line when sleeping

#define EYE_RADIUS_NORMAL 40
#define EYE_RADIUS_HAPPY  50

#define EYE_OFFSET_Y_BASE 70   
#define GAZE_SHIFT_AMT    25   
#define DIRECTION_UP      1    

// Colors
#define COLOR_NORMAL    lv_color_make(0, 255, 255)   // Cyan
#define COLOR_HAPPY     lv_color_make(255, 105, 180) // Hot Pink
#define COLOR_SLEEP     lv_color_make(50, 50, 50)    // Dim Grey
#define COLOR_BG        lv_color_make(0, 0, 0)       // Black

static lv_obj_t *left_eye;
static lv_obj_t *right_eye;
static lv_obj_t *left_lid;
static lv_obj_t *right_lid;
static bool is_rubbing = false;
static bool is_sleeping = false;

// --- WIFI EVENT HANDLER ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect(); // Retry endlessly
        ESP_LOGI(TAG, "Retrying WiFi connection...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// --- RTC / SNTP SETUP ---
static void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // Set Timezone
    setenv("TZ", TIME_ZONE, 1);
    tzset();
}

static void check_sleep_schedule(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // If time hasn't synced (year 1970), don't sleep
    if (timeinfo.tm_year < (2016 - 1900)) return;

    // Check Hours
    bool night_time = false;
    if (SLEEP_HOUR_START > SLEEP_HOUR_END) {
        // Example: 22 to 7. Night is if hour >= 22 OR hour < 7
        if (timeinfo.tm_hour >= SLEEP_HOUR_START || timeinfo.tm_hour < SLEEP_HOUR_END) {
            night_time = true;
        }
    } else {
        // Example: 0 to 8. Night is if hour >= 0 AND hour < 8
        if (timeinfo.tm_hour >= SLEEP_HOUR_START && timeinfo.tm_hour < SLEEP_HOUR_END) {
            night_time = true;
        }
    }

    if (night_time && !is_sleeping && !is_rubbing) {
        ESP_LOGI(TAG, "Yawn... Goodnight. (%d:%02d)", timeinfo.tm_hour, timeinfo.tm_min);
        is_sleeping = true;
        // Force eyes closed
        lv_anim_del_all();
        lv_obj_set_width(left_eye, EYE_W_SLEEP);
        lv_obj_set_width(right_eye, EYE_W_SLEEP);
        lv_obj_set_style_bg_color(left_eye, COLOR_SLEEP, 0);
        lv_obj_set_style_bg_color(right_eye, COLOR_SLEEP, 0);
        // Reset Alignment
        lv_obj_align(left_eye, LV_ALIGN_CENTER, 0, -EYE_OFFSET_Y_BASE);
        lv_obj_align(right_eye, LV_ALIGN_CENTER, 0, EYE_OFFSET_Y_BASE);
    } 
    else if (!night_time && is_sleeping) {
        ESP_LOGI(TAG, "Good Morning! (%d:%02d)", timeinfo.tm_hour, timeinfo.tm_min);
        is_sleeping = false;
        // Wake up eyes
        lv_obj_set_width(left_eye, EYE_W_NORMAL);
        lv_obj_set_width(right_eye, EYE_W_NORMAL);
        lv_obj_set_style_bg_color(left_eye, COLOR_NORMAL, 0);
        lv_obj_set_style_bg_color(right_eye, COLOR_NORMAL, 0);
    }
}

// --- ANIMATION HELPERS ---
int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// --- CURIOUS ANIMATION ---
static void anim_curious_cb(void * var, int32_t v) {
    int side = (int)var; 
    int32_t l_target_w = (side == 0) ? EYE_W_WIDE : EYE_W_NORMAL;
    int32_t r_target_w = (side == 1) ? EYE_W_WIDE : EYE_W_NORMAL;

    int32_t l_w = map(v, 0, 256, EYE_W_NORMAL, l_target_w);
    int32_t r_w = map(v, 0, 256, EYE_W_NORMAL, r_target_w);

    int32_t l_center_shift_x = ((l_w - EYE_W_NORMAL) / 2) * DIRECTION_UP; 
    int32_t r_center_shift_x = ((r_w - EYE_W_NORMAL) / 2) * DIRECTION_UP;

    int32_t gaze_target = (side == 0) ? -GAZE_SHIFT_AMT : GAZE_SHIFT_AMT;
    int32_t gaze_current = map(v, 0, 256, 0, gaze_target);

    lv_obj_set_width(left_eye, l_w);
    lv_obj_align(left_eye, LV_ALIGN_CENTER, l_center_shift_x, -EYE_OFFSET_Y_BASE + gaze_current);

    lv_obj_set_width(right_eye, r_w);
    lv_obj_align(right_eye, LV_ALIGN_CENTER, r_center_shift_x, EYE_OFFSET_Y_BASE + gaze_current);
}

static void trigger_curious(void) {
    int active_side = rand() % 2; 
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, (void*)active_side);
    lv_anim_set_values(&a, 0, 256);     
    lv_anim_set_time(&a, 200);            
    lv_anim_set_playback_delay(&a, 2000); 
    lv_anim_set_playback_time(&a, 200);   
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_exec_cb(&a, anim_curious_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// --- BLINK ANIMATION ---
static void anim_blink_cb(void * var, int32_t v) {
    lv_obj_set_width((lv_obj_t *)var, v);
    int32_t offset = (var == left_eye) ? -EYE_OFFSET_Y_BASE : EYE_OFFSET_Y_BASE;
    lv_obj_align((lv_obj_t *)var, LV_ALIGN_CENTER, 0, offset); 
}

static void trigger_blink(void) {
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

// --- BEHAVIOR TIMER ---
static void robot_behavior_timer(lv_timer_t * timer) {
    // 1. Check Schedule every tick (RTC Logic)
    check_sleep_schedule();

    // 2. If Sleeping or Rubbing, do not animate behavior
    if (is_rubbing || is_sleeping) return;
    
    // Prevent overlap
    if (lv_anim_get(left_eye, NULL) || lv_anim_get((void*)0, NULL) || lv_anim_get((void*)1, NULL)) {
        return; 
    }

    int dice = rand() % 100;
    if (dice < 5) trigger_blink();
    else if (dice == 50) {
        trigger_curious();
    }
}

// --- HAPPY / RUBBING LOGIC ---
static void set_eye_state(bool happy) {
    lv_anim_del_all(); 

    int32_t target_w = happy ? EYE_H_THICKNESS : EYE_W_NORMAL; 
    int32_t target_radius = happy ? EYE_RADIUS_HAPPY : EYE_RADIUS_NORMAL;
    lv_color_t target_color = happy ? COLOR_HAPPY : COLOR_NORMAL;
    int32_t lid_w = happy ? (EYE_H_THICKNESS / 2) : 0;

    // Wake up if we were sleeping
    if (happy && is_sleeping) {
        // Temporary wake up visuals
    }

    lv_obj_set_width(left_eye, target_w); 
    lv_obj_set_style_radius(left_eye, target_radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(left_eye, target_color, LV_PART_MAIN);
    lv_obj_set_width(left_lid, lid_w);
    lv_obj_align(left_lid, LV_ALIGN_LEFT_MID, 0, 0); 

    lv_obj_set_width(right_eye, target_w);
    lv_obj_set_style_radius(right_eye, target_radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(right_eye, target_color, LV_PART_MAIN);
    lv_obj_set_width(right_lid, lid_w);
    lv_obj_align(right_lid, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_align(left_eye, LV_ALIGN_CENTER, 0, -EYE_OFFSET_Y_BASE);
    lv_obj_align(right_eye, LV_ALIGN_CENTER, 0, EYE_OFFSET_Y_BASE);
}

static void screen_touch_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        is_rubbing = true;
        set_eye_state(true); // Happy
    } 
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        is_rubbing = false;
        
        // If it's night time, go back to sleep immediately after release
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Quick check to restore sleep state if needed
        bool night_time = false;
        if (SLEEP_HOUR_START > SLEEP_HOUR_END) {
             if (timeinfo.tm_hour >= SLEEP_HOUR_START || timeinfo.tm_hour < SLEEP_HOUR_END) night_time = true;
        } else {
             if (timeinfo.tm_hour >= SLEEP_HOUR_START && timeinfo.tm_hour < SLEEP_HOUR_END) night_time = true;
        }
        
        if(night_time && timeinfo.tm_year > (2016-1900)) {
            is_sleeping = true;
            lv_anim_del_all();
            lv_obj_set_width(left_eye, EYE_W_SLEEP);
            lv_obj_set_width(right_eye, EYE_W_SLEEP);
            lv_obj_set_style_bg_color(left_eye, COLOR_SLEEP, 0);
            lv_obj_set_style_bg_color(right_eye, COLOR_SLEEP, 0);
            lv_obj_align(left_eye, LV_ALIGN_CENTER, 0, -EYE_OFFSET_Y_BASE);
            lv_obj_align(right_eye, LV_ALIGN_CENTER, 0, EYE_OFFSET_Y_BASE);
        } else {
            set_eye_state(false); // Normal
        }
    }
}

// --- WIFI INIT ---
static void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// --- SETUP ---
void ui_create_robot_face(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_ALL, NULL);

    left_eye = lv_obj_create(scr);
    lv_obj_remove_style_all(left_eye);
    lv_obj_set_size(left_eye, EYE_W_NORMAL, EYE_H_THICKNESS);
    lv_obj_set_style_bg_color(left_eye, COLOR_NORMAL, 0);
    lv_obj_set_style_bg_opa(left_eye, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(left_eye, EYE_RADIUS_NORMAL, 0);
    lv_obj_align(left_eye, LV_ALIGN_CENTER, 0, -EYE_OFFSET_Y_BASE);
    lv_obj_clear_flag(left_eye, LV_OBJ_FLAG_CLICKABLE);

    left_lid = lv_obj_create(left_eye);
    lv_obj_remove_style_all(left_lid);
    lv_obj_set_size(left_lid, 0, EYE_H_THICKNESS); 
    lv_obj_set_style_bg_color(left_lid, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(left_lid, LV_OPA_COVER, 0);
    lv_obj_align(left_lid, LV_ALIGN_LEFT_MID, 0, 0); 
    lv_obj_clear_flag(left_lid, LV_OBJ_FLAG_CLICKABLE);

    right_eye = lv_obj_create(scr);
    lv_obj_remove_style_all(right_eye);
    lv_obj_set_size(right_eye, EYE_W_NORMAL, EYE_H_THICKNESS);
    lv_obj_set_style_bg_color(right_eye, COLOR_NORMAL, 0);
    lv_obj_set_style_bg_opa(right_eye, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(right_eye, EYE_RADIUS_NORMAL, 0);
    lv_obj_align(right_eye, LV_ALIGN_CENTER, 0, EYE_OFFSET_Y_BASE);
    lv_obj_clear_flag(right_eye, LV_OBJ_FLAG_CLICKABLE);

    right_lid = lv_obj_create(right_eye);
    lv_obj_remove_style_all(right_lid);
    lv_obj_set_size(right_lid, 0, EYE_H_THICKNESS);
    lv_obj_set_style_bg_color(right_lid, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(right_lid, LV_OPA_COVER, 0);
    lv_obj_align(right_lid, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(right_lid, LV_OBJ_FLAG_CLICKABLE);

    lv_timer_create(robot_behavior_timer, 100, NULL);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. START WIFI
    wifi_init_sta();
    
    // 2. START SNTP (RTC)
    initialize_sntp();

    lv_display_t *disp = bsp_display_start();
    if (disp) {
        bsp_display_brightness_set(45); 
    }

    bsp_display_lock(0);
    ui_create_robot_face();
    bsp_display_unlock();
    
    ESP_LOGI(TAG, "Robot RTC Active (Sleeps 22:00 - 07:00).");
}
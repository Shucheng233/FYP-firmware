#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool power;
    int brightness;     // 0~100
    int color_temp;     // 2700~6500
} light_state_t;

typedef struct {
    bool power;
    int speed;          // 0~100
} fan_state_t;

typedef struct {
    light_state_t livingroom_light;
    light_state_t bedroom_light;
    fan_state_t   livingroom_fan;
    fan_state_t   bedroom_fan;
} device_state_store_t;

// init
void device_control_init(void);

// state
void device_control_get_state(device_state_store_t *state);

// fan
void fan_on(int fan_id);
void fan_off(int fan_id);
void fan_set_speed(int fan_id, int speed_percent);

// light
void light_on(int light_id);
void light_off(int light_id);
void light_set_rgb(int light_id, uint8_t r, uint8_t g, uint8_t b);

#endif
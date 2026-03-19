#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H
#include <stdint.h>

void device_control_init(void);

// -------- FAN --------
void fan_on(int fan_id);
void fan_off(int fan_id);
void fan_set_speed(int fan_id, int duty);

// -------- LIGHT --------
void light_on(int light_id);
void light_off(int light_id);
void light_set_rgb(int light_id, uint8_t r, uint8_t g, uint8_t b);

#endif
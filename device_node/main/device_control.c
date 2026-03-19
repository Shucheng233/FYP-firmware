//设备控制代码：light, fan, GPIO, PWM, Relay
//livingroom_fan     → GPIO4  (PWM)
//bedroom_fan        → GPIO5  (PWM)
//livingroom_light   → WS2812B
//bedroom_light      → WS2812B

#include "device_control.h"
#include "driver/ledc.h"
#include "led_strip.h"

#define FAN1_GPIO 4
#define FAN2_GPIO 5
#define LIGHT1_GPIO 6
#define LIGHT2_GPIO 7

//WS2812B
static led_strip_handle_t light1;
static led_strip_handle_t light2;

//PWM初始化
static void fan_pwm_init()
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t fan1 = {
        .gpio_num = FAN1_GPIO,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .speed_mode = LEDC_LOW_SPEED_MODE
    };

    ledc_channel_config_t fan2 = {
        .gpio_num = FAN2_GPIO,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .speed_mode = LEDC_LOW_SPEED_MODE
    };

    ledc_channel_config(&fan1);
    ledc_channel_config(&fan2);
}

//light初始化
static void light_init()
{
    led_strip_config_t strip_config1 = {
        .strip_gpio_num = LIGHT1_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config1 = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };

    led_strip_new_rmt_device(&strip_config1, &rmt_config1, &light1);

    led_strip_config_t strip_config2 = {
        .strip_gpio_num = LIGHT2_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config2 = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    led_strip_new_rmt_device(&strip_config2, &rmt_config2, &light2);
}

//统一初始化入口
void device_control_init(void)
{
    fan_pwm_init();
    light_init();
}

//fan控制
void fan_set_speed(int fan_id, int duty)
{
    int channel;

    if (fan_id == 1)
        channel = LEDC_CHANNEL_0;
    else
        channel = LEDC_CHANNEL_1;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

//light控制
void light_set_rgb(int light_id, uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_handle_t strip = (light_id == 1) ? light1 : light2;

    led_strip_set_pixel(strip, 0, r, g, b);
    led_strip_refresh(strip);
}

void light_on(int light_id)
{
    light_set_rgb(light_id, 255, 255, 255); // 白光
}

void light_off(int light_id)
{
    light_set_rgb(light_id, 0, 0, 0);
}

//封装开关（给上层用）
void fan_on(int fan_id)
{
    fan_set_speed(fan_id, 255);
}

void fan_off(int fan_id)
{
    fan_set_speed(fan_id, 0);
}


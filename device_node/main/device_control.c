//设备控制代码：light, fan, GPIO, PWM, Relay
//livingroom_fan     → GPIO4  (PWM)
//bedroom_fan        → GPIO5  (PWM)
//livingroom_light   → WS2812B
//bedroom_light      → WS2812B


#include "device_control.h"
#include "driver/ledc.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/rmt.h"
#include <string.h>

#define TAG "DEVICE_CTRL"

#define FAN1_GPIO   4
#define FAN2_GPIO   5
#define LIGHT1_GPIO 6
#define LIGHT2_GPIO 7

#define PWM_FREQ_HZ         5000
#define PWM_RESOLUTION      LEDC_TIMER_8_BIT
#define PWM_TIMER           LEDC_TIMER_0
#define PWM_MODE            LEDC_LOW_SPEED_MODE
#define FAN1_CHANNEL        LEDC_CHANNEL_0
#define FAN2_CHANNEL        LEDC_CHANNEL_1
#define PWM_MAX_DUTY        255

#define DEFAULT_BRIGHTNESS  100
#define DEFAULT_COLOR_TEMP  4000
#define DEFAULT_FAN_SPEED   0

static led_strip_t *light1 = NULL;
static led_strip_t *light2 = NULL;
static bool g_device_control_initialized = false;

static device_state_store_t g_state;

static void state_store_init_defaults(void)
{
    memset(&g_state, 0, sizeof(g_state));

    g_state.livingroom_light.power = false;
    g_state.livingroom_light.brightness = DEFAULT_BRIGHTNESS;
    g_state.livingroom_light.color_temp = DEFAULT_COLOR_TEMP;

    g_state.bedroom_light.power = false;
    g_state.bedroom_light.brightness = DEFAULT_BRIGHTNESS;
    g_state.bedroom_light.color_temp = DEFAULT_COLOR_TEMP;

    g_state.livingroom_fan.power = false;
    g_state.livingroom_fan.speed = DEFAULT_FAN_SPEED;

    g_state.bedroom_fan.power = false;
    g_state.bedroom_fan.speed = DEFAULT_FAN_SPEED;
}

//4.1 //初始化 LEDC timer；初始化两个风扇 PWM channel；默认 duty = 0；做错误检查
static esp_err_t fan_pwm_init(void)
{
    esp_err_t err;

    ledc_timer_config_t timer_config = {
        .speed_mode = PWM_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fan_pwm_init: ledc_timer_config failed: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t fan1_config = {
        .gpio_num = FAN1_GPIO,
        .speed_mode = PWM_MODE,
        .channel = FAN1_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    err = ledc_channel_config(&fan1_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fan_pwm_init: fan1 channel config failed: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t fan2_config = {
        .gpio_num = FAN2_GPIO,
        .speed_mode = PWM_MODE,
        .channel = FAN2_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    err = ledc_channel_config(&fan2_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fan_pwm_init: fan2 channel config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Fan PWM initialized. FAN1 GPIO=%d, FAN2 GPIO=%d", FAN1_GPIO, FAN2_GPIO);
    return ESP_OK;
}

//4.2 //初始化 WS2812B 灯带；创建两个 RMT device；默认全灭；做错误检查
static esp_err_t light_init(void)
{
    light1 = led_strip_init(RMT_CHANNEL_0, LIGHT1_GPIO, 1);
    if (light1 == NULL) {
        ESP_LOGE(TAG, "light_init: create light1 failed");
        return ESP_FAIL;
    }

    light2 = led_strip_init(RMT_CHANNEL_1, LIGHT2_GPIO, 1);
    if (light2 == NULL) {
        ESP_LOGE(TAG, "light_init: create light2 failed");
        return ESP_FAIL;
    }

    if (light1->clear(light1, 100) != ESP_OK) {
        ESP_LOGE(TAG, "light_init: clear light1 failed");
        return ESP_FAIL;
    }

    if (light2->clear(light2, 100) != ESP_OK) {
        ESP_LOGE(TAG, "light_init: clear light2 failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Lights initialized. LIGHT1 GPIO=%d, LIGHT2 GPIO=%d", LIGHT1_GPIO, LIGHT2_GPIO);
    return ESP_OK;
}

//百分比转 PWM duty
static int percent_to_duty(int percent)
{
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    return (percent * PWM_MAX_DUTY) / 100;
}

//根据 fan_id 选 channel
static ledc_channel_t get_fan_channel(int fan_id)
{
    if (fan_id == 1) {
        return FAN1_CHANNEL;
    }
    return FAN2_CHANNEL;
}




//4.3 //防止重复初始化；初始化 state；初始化 fan PWM；初始化 lights；设置初始化标志
void device_control_init(void)
{
    esp_err_t err;

    if (g_device_control_initialized) {
        ESP_LOGW(TAG, "device_control_init: already initialized, skip");
        return;
    }

    ESP_LOGI(TAG, "device_control_init: start");

    state_store_init_defaults();

    err = fan_pwm_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "device_control_init: fan_pwm_init failed");
        return;
    }

    err = light_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "device_control_init: light_init failed");
        return;
    }

    g_device_control_initialized = true;

    ESP_LOGI(TAG, "device_control_init: success");
    ESP_LOGI(TAG, "Default state -> LR light: off, BR light: off, LR fan: off, BR fan: off");
}

void device_control_get_state(device_state_store_t *state)
{
    if (state == NULL) {
        return;
    }
    *state = g_state;
}

void fan_set_speed(int fan_id, int speed_percent)
{
    ledc_channel_t channel;
    int duty;

    if (!g_device_control_initialized) {
        ESP_LOGE(TAG, "fan_set_speed: device control not initialized");
        return;
    }

    if (fan_id != 1 && fan_id != 2) {
        ESP_LOGW(TAG, "fan_set_speed: invalid fan_id=%d", fan_id);
        return;
    }

    if (speed_percent < 0) {
        speed_percent = 0;
    }
    if (speed_percent > 100) {
        speed_percent = 100;
    }

    channel = get_fan_channel(fan_id);
    duty = percent_to_duty(speed_percent);

    ledc_set_duty(PWM_MODE, channel, duty);
    ledc_update_duty(PWM_MODE, channel);

    if (fan_id == 1) {
        g_state.livingroom_fan.speed = speed_percent;
        g_state.livingroom_fan.power = (speed_percent > 0);
    } else {
        g_state.bedroom_fan.speed = speed_percent;
        g_state.bedroom_fan.power = (speed_percent > 0);
    }

    ESP_LOGI(TAG, "fan_set_speed: fan%d speed=%d%% duty=%d", fan_id, speed_percent, duty);
}

void fan_on(int fan_id)
{
    fan_set_speed(fan_id, 100);
}

void fan_off(int fan_id)
{
    fan_set_speed(fan_id, 0);
}

//灯控制
void light_set_rgb(int light_id, uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_t *strip = NULL;
    esp_err_t err;

    if (!g_device_control_initialized) {
        ESP_LOGE(TAG, "light_set_rgb: device control not initialized");
        return;
    }

    if (light_id == 1) {
        strip = light1;
    } else if (light_id == 2) {
        strip = light2;
    } else {
        ESP_LOGW(TAG, "light_set_rgb: invalid light_id=%d", light_id);
        return;
    }

    if (strip == NULL) {
        ESP_LOGE(TAG, "light_set_rgb: strip not initialized");
        return;
    }

    err = strip->set_pixel(strip, 0, r, g, b);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "light_set_rgb: set_pixel failed");
        return;
    }

    err = strip->refresh(strip, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "light_set_rgb: refresh failed");
        return;
    }

    if (light_id == 1) {
        g_state.livingroom_light.power = (r || g || b);
        g_state.livingroom_light.brightness = (r || g || b) ? 100 : 0;
    } else {
        g_state.bedroom_light.power = (r || g || b);
        g_state.bedroom_light.brightness = (r || g || b) ? 100 : 0;
    }

    ESP_LOGI(TAG, "light_set_rgb: light%d rgb=(%d,%d,%d)", light_id, r, g, b);
}

void light_on(int light_id)
{
    light_set_rgb(light_id, 255, 255, 255);

    if (light_id == 1) {
        g_state.livingroom_light.brightness = 100;
    } else if (light_id == 2) {
        g_state.bedroom_light.brightness = 100;
    }
}

void light_off(int light_id)
{
    led_strip_t *strip = NULL;
    esp_err_t err;

    if (!g_device_control_initialized) {
        ESP_LOGE(TAG, "light_off: device control not initialized");
        return;
    }

    if (light_id == 1) {
        strip = light1;
    } else if (light_id == 2) {
        strip = light2;
    } else {
        ESP_LOGW(TAG, "light_off: invalid light_id=%d", light_id);
        return;
    }

    if (strip == NULL) {
        ESP_LOGE(TAG, "light_off: strip not initialized");
        return;
    }

    err = strip->clear(strip, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "light_off: clear failed");
        return;
    }

    if (light_id == 1) {
        g_state.livingroom_light.power = false;
        g_state.livingroom_light.brightness = 0;
    } else {
        g_state.bedroom_light.power = false;
        g_state.bedroom_light.brightness = 0;
    }

    ESP_LOGI(TAG, "light_off: light%d off", light_id);
}
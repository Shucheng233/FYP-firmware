#include "device_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    device_state_store_t state;

    ESP_LOGI(TAG, "app_main start");

    device_control_init();

    // 1. 测试客厅风扇开/关
    ESP_LOGI(TAG, "Test 1: livingroom fan ON");
    fan_on(1);
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Test 1: livingroom fan OFF");
    fan_off(1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 2. 测试卧室风扇开/关
    ESP_LOGI(TAG, "Test 2: bedroom fan ON");
    fan_on(2);
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Test 2: bedroom fan OFF");
    fan_off(2);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 3. 测试客厅灯开/关
    ESP_LOGI(TAG, "Test 3: livingroom light ON");
    light_on(1);
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Test 3: livingroom light OFF");
    light_off(1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 4. 测试卧室灯开/关
    ESP_LOGI(TAG, "Test 4: bedroom light ON");
    light_on(2);
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Test 4: bedroom light OFF");
    light_off(2);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 5. 读取当前状态
    device_control_get_state(&state);

    ESP_LOGI(TAG, "Final state:");
    ESP_LOGI(TAG, "LR fan -> power=%d speed=%d",
             state.livingroom_fan.power,
             state.livingroom_fan.speed);

    ESP_LOGI(TAG, "BR fan -> power=%d speed=%d",
             state.bedroom_fan.power,
             state.bedroom_fan.speed);

    ESP_LOGI(TAG, "LR light -> power=%d brightness=%d color_temp=%d",
             state.livingroom_light.power,
             state.livingroom_light.brightness,
             state.livingroom_light.color_temp);

    ESP_LOGI(TAG, "BR light -> power=%d brightness=%d color_temp=%d",
             state.bedroom_light.power,
             state.bedroom_light.brightness,
             state.bedroom_light.color_temp);

    // 6. 循环测试：两个风扇 + 两个灯
    while (1)
    {
        ESP_LOGI(TAG, "Loop test: all ON");
        fan_on(1);
        fan_on(2);
        light_on(1);
        light_on(2);
        vTaskDelay(pdMS_TO_TICKS(3000));

        ESP_LOGI(TAG, "Loop test: all OFF");
        fan_off(1);
        fan_off(2);
        light_off(1);
        light_off(2);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
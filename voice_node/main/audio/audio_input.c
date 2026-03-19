//ES8311
//麦克风
//I2S采集

#include "audio/audio_input.h"
#include "drivers/i2s_driver.h"
#include "drivers/es8311.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/i2s.h"

#define I2C_SDA 8
#define I2C_SCL 9

static const char *TAG = "AUDIO_IN";

static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

static void audio_capture_task(void *arg)
{
    int16_t buffer[1024];
    size_t bytes_read;

    while (1)
    {
        esp_err_t ret = i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Read %d bytes", bytes_read);
        }
        else
        {
            ESP_LOGE(TAG, "I2S read error");
        }
    }
}

void audio_input_init(void)
{
    ESP_LOGI(TAG, "Init audio input...");

    i2c_init();        // 初始化I2C（控制ES8311）
    es8311_init();     // 初始化codec
    i2s_init();        // 初始化I2S

    xTaskCreate(audio_capture_task, "audio_task", 4096, NULL, 5, NULL);
}
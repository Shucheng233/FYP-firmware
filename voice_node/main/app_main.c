#include "audio/audio_input.h"
#include "audio/audio_output.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    audio_input_init();
    audio_output_init();

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
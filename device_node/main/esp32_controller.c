#include "device_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    device_control_init();

    while (1)
    {
        fan_on(1);   // 客厅风扇
        fan_on(2);   // 卧室风扇

        vTaskDelay(3000 / portTICK_PERIOD_MS);

        fan_off(1);
        fan_off(2);

        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

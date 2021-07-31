#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "TASK: dust"

#include "pms7003.h"

#include "dust_sensor.h"

#define DUST_MAX_FAILS 20

void dust_sensor_task()
{

    ESP_ERROR_CHECK(pms_init(DUST_PIN_TX, DUST_PIN_RX, UART_NUM_2));

    pms_set_passive_mode();

    for (;;)
    {
        uint8_t fails_count = 0;

        pms_values_t pms_values;

        ESP_ERROR_CHECK(pms_fill_values(&pms_values));

        while (xSemaphoreTake(dust_values.lock, 1000 / portTICK_PERIOD_MS) != pdTRUE)
        {
            fails_count++;
            if (fails_count > DUST_MAX_FAILS)
            {
                ESP_LOGE(LOG_TAG, "can't take lock on dust values, panic");
                return;
            }
        };

        dust_values.pm100 = pms_values.pm100;
        dust_values.pm25 = pms_values.pm25;

        dust_values.updated = true;

        xSemaphoreGive(dust_values.lock);

        ESP_LOGV(LOG_TAG, "updated pm25 is %d", dust_values.pm25);
        ESP_LOGV(LOG_TAG, "updated pm100 is %d", dust_values.pm100);

        vTaskDelay(DUST_TASK_DELAY / portTICK_PERIOD_MS);
    }
}

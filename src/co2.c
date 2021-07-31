#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "TASK: co2"

#include "dust_sensor.h"

#define CO2_MAX_FAILS 20

void co2_sensor_task()
{
    /*
    This is a mh-z19b procedure
    */

    ESP_ERROR_CHECK(mhz19_init(CO2_PIN_TX, CO2_PIN_RX, UART_NUM_1));

    for (;;)
    {
        mhz19_values_t values;
        uint8_t fails_count = 0;

        ESP_ERROR_CHECK(mhz19_fill_values(&values));

        while (xSemaphoreTake(co2_values.lock, 1000 / portTICK_PERIOD_MS) != pdTRUE)
        {
            fails_count++;
            if (fails_count > CO2_MAX_FAILS)
            {
                ESP_LOGE(LOG_TAG, "can't take lock on co2 values, panic");
                return;
            }
        };

        co2_values.ppm = values.ppm;
        co2_values.updated = true;

        ESP_LOGV(LOG_TAG, "updated co2 ppm is %d", co2_values.ppm);

        xSemaphoreGive(co2_values.lock);

        vTaskDelay(CO2_TASK_DELAY / portTICK_PERIOD_MS);
    }
}

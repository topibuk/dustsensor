#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "TASK: pressure"

#include "bmp.h"

#include "dust_sensor.h"

#define BMP_MAX_FAILS 20

void bmp_task()
{

    ESP_ERROR_CHECK(bmp_init(BMP_SDA_PIN, BMP_SCL_PIN, I2C_NUM_0));

    for (;;)
    {

        bmp_values_t values;

        ESP_ERROR_CHECK(bmp_fill_values(&values));

        int fails_count = 0;

        while (xSemaphoreTake(bmp_values.lock, 1000 / portTICK_PERIOD_MS) != pdTRUE)
        {
            fails_count++;
            if (fails_count > BMP_MAX_FAILS)
            {
                ESP_LOGE(LOG_TAG, "can't take lock on bmp values, panic");
                return;
            }
        };

        /*
        Adjust temperature.
        Use Less Squares method for a series of real measurements
        Approximate result with the line: Treal = A * Tmeasured + B
        Constants A & B are in config header file
        */

        bmp_values.temp = TEMP_K_A * values.temp + TEMP_K_B;
        bmp_values.pres = values.pres;
        bmp_values.updated = true;

        ESP_LOGV(LOG_TAG, "T float: %f", values.temp);
        ESP_LOGV(LOG_TAG, "T float adjusted: %f", bmp_values.temp);

        ESP_LOGV(LOG_TAG, "P float: %f", values.pres);

        xSemaphoreGive(bmp_values.lock);

        vTaskDelay(BMP_TASK_DELAY / portTICK_PERIOD_MS);
    }
}

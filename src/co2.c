#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "TASK: co2"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "dust_sensor.h"

#define BUF_SIZE UART_FIFO_LEN * 2

#define CO2_PIN_RX GPIO_NUM_26
#define CO2_PIN_TX GPIO_NUM_27

#define CO2_MAX_FAILS 20

#define CO2_TASK_DELAY 10000 //microseconds

static char cmd_co2_read[] = {
    0xFF,
    0x01,
    0x86,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x79,
};

void co2_sensor_task()
{
    /*
    This is a mh-z19b procedure
    */

    uart_config_t co2_config = {

        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE

    };

    uart_param_config(UART_NUM_1, &co2_config);
    uart_set_pin(UART_NUM_1, CO2_PIN_TX, CO2_PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, BUF_SIZE, BUF_SIZE, 0, NULL, 0);

    for (;;)
    {

        int8_t res;
        uint8_t data[9];
        uint8_t count = 0;
        uint8_t fails_count = 0;

        res = uart_write_bytes(UART_NUM_1, (const char *)cmd_co2_read, sizeof(cmd_co2_read));

        if (res < 0)
        {
            ESP_LOGE(LOG_TAG, "can't write to co2 sensor, panic");
            return;
        }

        while (count < sizeof(data))
        {
            count = uart_read_bytes(UART_NUM_1, data + count, sizeof(data) - count, 200 / portTICK_PERIOD_MS);
            if (count <= 0)
            {
                fails_count++;
            }

            if (fails_count > CO2_MAX_FAILS)
            {
                ESP_LOGW(LOG_TAG, "unable to read from co2 sensor");
                break;
            }
        }

        fails_count = 0;
        while (xSemaphoreTake(co2_values.lock, 1000 / portTICK_PERIOD_MS) != pdTRUE)
        {
            fails_count++;
            if (fails_count > CO2_MAX_FAILS)
            {
                ESP_LOGE(LOG_TAG, "can't take lock on co2 values, panic");
                return;
            }
        };

        co2_values.ppm = (uint16_t)((uint16_t)data[2] << 8 | (uint16_t)data[3]);
        co2_values.updated = true;

        ESP_LOGV(LOG_TAG, "updated co2 ppm is %d", co2_values.ppm);

        xSemaphoreGive(co2_values.lock);

        vTaskDelay(CO2_TASK_DELAY / portTICK_PERIOD_MS);
    }
}

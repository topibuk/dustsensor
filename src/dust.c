#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "TASK: dust"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "dust_sensor.h"

#define DUST_RX_BUF_SIZE UART_FIFO_LEN * 2
#define DUST_TX_BUF_SIZE 0

#define DUST_MAX_FAILS 20

static char cmd_dust_set_passive_mode[] = {0x42,
                                           0x4D,
                                           0xE1,
                                           0x00,
                                           0x00,
                                           0x01,
                                           0x70};

// 0x42 + 0x4D + 0xE1 + 0x00 + 0x00 + 0x01 = 0x170 = 0x1 << 8 + 0x70

static char cmd_dust_read[] = {0x42,
                               0x4D,
                               0xE2,
                               0x00,
                               0x00,
                               0x01,
                               0x71};

// 0x42 + 0x4D + 0xE2 + 0x00 + 0x00 + 0x01 = 0x171 = 0x1 << 8 + 0x71

void dust_sensor_task()
{
    /*
    This is a PMS 7003 procedure
    */

    int8_t res;
    uint8_t data[32];
    uint8_t count = 0;
    uint8_t fails_count = 0;

    uart_config_t dust_config = {

        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE

    };

    uart_param_config(UART_NUM_2, &dust_config);
    uart_set_pin(UART_NUM_2, DUST_PIN_TX, DUST_PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, DUST_RX_BUF_SIZE, DUST_TX_BUF_SIZE, 0, NULL, 0);

    uart_write_bytes(UART_NUM_2, (const char *)cmd_dust_set_passive_mode, sizeof(cmd_dust_set_passive_mode));

    for (;;)
    {

        res = uart_write_bytes(UART_NUM_2, (const char *)cmd_dust_read, sizeof(cmd_dust_read));

        if (res < 0)
        {
            ESP_LOGE(LOG_TAG, "can't write to dust sensor, panic");
            return;
        }

        count = 0;

        while (count < sizeof(data))
        {
            count = uart_read_bytes(UART_NUM_2, data + count, sizeof(data) - count, 200 / portTICK_PERIOD_MS);
            ESP_LOGV(LOG_TAG, "read %i bytes", count);
            if (count <= 0)
            {
                fails_count++;
            }

            if (fails_count > DUST_MAX_FAILS)
            {
                ESP_LOGW(LOG_TAG, "unable to read from dust sensor");
                break;
            }
        }

        fails_count = 0;

        while (xSemaphoreTake(dust_values.lock, 1000 / portTICK_PERIOD_MS) != pdTRUE)
        {
            fails_count++;
            if (fails_count > DUST_MAX_FAILS)
            {
                ESP_LOGE(LOG_TAG, "can't take lock on dust values, panic");
                return;
            }
        };

        fails_count = 0;

        dust_values.pm25 = (data[12] << 8) + data[13];
        dust_values.pm100 = (data[14] << 8) + data[15];
        dust_values.updated = true;

        ESP_LOGV(LOG_TAG, "updated pm25 is %d", dust_values.pm25);
        ESP_LOGV(LOG_TAG, "updated pm100 is %d", dust_values.pm100);

        xSemaphoreGive(dust_values.lock);

        vTaskDelay(DUST_TASK_DELAY / portTICK_PERIOD_MS);
    }
}

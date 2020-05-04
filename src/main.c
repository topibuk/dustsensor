#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#define PIN_RX GPIO_NUM_16
#define PIN_TX GPIO_NUM_17
#define PIN_RTS UART_PIN_NO_CHANGE
#define PIN_CTS UART_PIN_NO_CHANGE

#define BUF_SIZE UART_FIFO_LEN * 2

char data[] = "Heartbeat\n";

void dump_data(const uint8_t *data)
{
    char message[100];
    uint16_t current_value;
    for (int i = 0; i < 16; i++)
    {
        current_value = (data[i * 2] << 8) + (data[i * 2 + 1]);
        sprintf(message, " %05u", current_value);
        uart_write_bytes(UART_NUM_0, (const char *)message, strlen(message));
    }
    sprintf(message, "\n");
    uart_write_bytes(UART_NUM_0, (const char *)message, strlen(message));
}

static void dust_sensor_task()
{
    /*
    This is a PMS 7003 procedure
    */
    uart_config_t out_config = {

        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE

    };

    uart_config_t in_config = {

        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE

    };

    uart_param_config(UART_NUM_0, &out_config);
    uart_param_config(UART_NUM_2, &in_config);

    uart_set_pin(UART_NUM_2, PIN_TX, PIN_RX, PIN_RTS, PIN_CTS);

    uart_driver_install(UART_NUM_0, BUF_SIZE, 0, 0, NULL, 0);
    uart_driver_install(UART_NUM_2, BUF_SIZE, BUF_SIZE, 0, NULL, 0);

    uint8_t b[100];
    int result;

    char cmd_passive_mode[] = {0x42,
                               0x4D,
                               0xE1,
                               0x00,
                               0x00,
                               0x01,
                               0x70};

    // 0x42 + 0x4D + 0xE1 + 0x00 + 0x00 + 0x01 = 0x170 = 0x1 << 8 + 0x70

    uart_write_bytes(UART_NUM_2, (const char *)cmd_passive_mode, 7);

    for (;;)
    {

        char cmd_read[] = {0x42,
                           0x4D,
                           0xE2,
                           0x00,
                           0x00,
                           0x01,
                           0x71};

        // 0x42 + 0x4D + 0xE2 + 0x00 + 0x00 + 0x01 = 0x171 = 0x1 << 8 + 0x71

        uart_write_bytes(UART_NUM_2, (const char *)cmd_read, 7);

        result = uart_read_bytes(UART_NUM_2, b, BUF_SIZE, 20);

        if (result >= 0)
        {
            dump_data(b);
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    xTaskCreate(dust_sensor_task, "dust_sensor_task", 1024, NULL, 10, NULL);
}
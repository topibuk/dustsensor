#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define PIN_RX GPIO_NUM_16
#define PIN_TX GPIO_NUM_17
#define PIN_RTS UART_PIN_NO_CHANGE
#define PIN_CTS UART_PIN_NO_CHANGE

#define LOG_TAG "main"

#define BUF_SIZE UART_FIFO_LEN * 2

typedef struct DustSens_data
{
    uint16_t pm01_consentration_standard;
    uint16_t pm25_consentration_standard;
    uint16_t pm10_consentration_standard;
    uint16_t pm01_consentration_atmo;
    uint16_t pm25_consentration_atmo;
    uint16_t pm10_consentration_atmo;
    uint16_t part_number03;
    uint16_t part_number05;
    uint16_t part_number10;
    uint16_t part_number25;
    uint16_t part_number50;
    uint16_t part_number100;
} DustSens_t;

DustSens_t dust_mesurement;

void parse_data(const uint8_t *data, uint8_t length)
{

    if (length != 32)
    {
        return;
    }

    dust_mesurement.pm01_consentration_standard = (data[4] << 8) + data[5];
    dust_mesurement.pm25_consentration_standard = (data[6] << 8) + data[7];
    dust_mesurement.pm10_consentration_standard = (data[8] << 8) + data[9];
    dust_mesurement.pm01_consentration_atmo = (data[10] << 8) + data[11];
    dust_mesurement.pm25_consentration_atmo = (data[12] << 8) + data[13];
    dust_mesurement.pm10_consentration_atmo = (data[14] << 8) + data[15];
    dust_mesurement.part_number03 = (data[16] << 8) + data[17];
    dust_mesurement.part_number05 = (data[18] << 8) + data[19];
    dust_mesurement.part_number10 = (data[20] << 8) + data[21];
    dust_mesurement.part_number25 = (data[22] << 8) + data[23];
    dust_mesurement.part_number50 = (data[24] << 8) + data[25];
    dust_mesurement.part_number100 = (data[26] << 8) + data[27];

    ESP_LOGI(LOG_TAG, "pm 2.5 atmo concentration is %05d", dust_mesurement.pm25_consentration_atmo);
    ESP_LOGI(LOG_TAG, "pm 10  atmo concentration is %05d", dust_mesurement.pm10_consentration_atmo);

    ESP_LOGI(LOG_TAG, "pm 2.5 stan concentration is %05d", dust_mesurement.pm25_consentration_standard);
    ESP_LOGI(LOG_TAG, "pm 10  stan concentration is %05d", dust_mesurement.pm10_consentration_standard);
}

static void dust_sensor_task()
{
    /*
    This is a PMS 7003 procedure
    */

    uart_config_t in_config = {

        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE

    };

    uart_param_config(UART_NUM_2, &in_config);
    uart_set_pin(UART_NUM_2, PIN_TX, PIN_RX, PIN_RTS, PIN_CTS);
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
            parse_data(b, 32);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
    }
}

void app_main()
{
    xTaskCreate(dust_sensor_task, "dust_sensor_task", 1024, NULL, 10, NULL);
}
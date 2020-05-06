#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "secrets.h"

#define PIN_RX GPIO_NUM_16
#define PIN_TX GPIO_NUM_17
#define PIN_RTS UART_PIN_NO_CHANGE
#define PIN_CTS UART_PIN_NO_CHANGE

#define LOG_TAG "main"

#define BUF_SIZE UART_FIFO_LEN * 2

#define WIFI_CONNECT_MAXIMUM_RETRY 100

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

SemaphoreHandle_t dust_semaphore;

uint8_t wifi_connect_retry_counter;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* 
- connected to the AP with an IP
- maximum number of retries reached
 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

void parse_dust_data(const uint8_t *data, uint8_t length)
{

    if (length != 32)
    {
        return;
    }

    while (xSemaphoreTake(dust_semaphore, 100 / portTICK_PERIOD_MS) != pdTRUE)
    {
        ESP_LOGE(LOG_TAG, "can't get lock on dust_semaphore");
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

    xSemaphoreGive(dust_semaphore);
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

    char cmd_read[] = {0x42,
                       0x4D,
                       0xE2,
                       0x00,
                       0x00,
                       0x01,
                       0x71};

    // 0x42 + 0x4D + 0xE2 + 0x00 + 0x00 + 0x01 = 0x171 = 0x1 << 8 + 0x71

    uart_write_bytes(UART_NUM_2, (const char *)cmd_passive_mode, 7);

    for (;;)
    {

        uart_write_bytes(UART_NUM_2, (const char *)cmd_read, 7);

        result = uart_read_bytes(UART_NUM_2, b, BUF_SIZE, 20);

        if (result >= 0)
        {
            parse_dust_data(b, result);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
            wifi_connect_retry_counter++;
            ESP_LOGI(LOG_TAG, "connecting to the AP");
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            if (wifi_connect_retry_counter < WIFI_CONNECT_MAXIMUM_RETRY)
            {
                esp_wifi_connect();
                wifi_connect_retry_counter++;
                ESP_LOGI(LOG_TAG, "connecting to the AP");
            }
            else
            {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI(LOG_TAG, "connect to the AP fail");
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(LOG_TAG, "got ip:%s",
                     ip4addr_ntoa(&event->ip_info.ip));
            wifi_connect_retry_counter = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

void start_wifi()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(LOG_TAG, "wifi in station mode started");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(LOG_TAG, "connected to ap SSID:%s", WIFI_SSID);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(LOG_TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    }
    else
    {
        ESP_LOGE(LOG_TAG, "UNEXPECTED EVENT");
    }
}

void app_main()
{
    dust_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(dust_semaphore);

    //Initialize NVS for internal use of WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(LOG_TAG, "starting WiFi in station mode");

    start_wifi();

    //    xTaskCreate(dust_sensor_task, "dust_sensor_task", 1024, NULL, 10, NULL);
}
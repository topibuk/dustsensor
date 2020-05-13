#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "secrets.h"
#include "mqtt_client.h"
#include "stdatomic.h"
#include "bmp280.h"

#define PIN_RX GPIO_NUM_16
#define PIN_TX GPIO_NUM_17
#define PIN_RTS UART_PIN_NO_CHANGE
#define PIN_CTS UART_PIN_NO_CHANGE

#define SDA_PIN GPIO_NUM_25
#define SCL_PIN GPIO_NUM_33
#define I2C_ACK 0x0
#define I2C_NACK 0x1

#define BMP280_I2C_ADDRESS 0x76

#define LOG_TAG "main"

#define BUF_SIZE UART_FIFO_LEN * 2

#define WIFI_CONNECT_MAXIMUM_RETRY 100

SemaphoreHandle_t dust_semaphore;

uint8_t wifi_connect_retry_counter;

atomic_char mqtt_connected = 0;

esp_mqtt_client_handle_t mqtt_client;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* 
- connected to the AP with an IP
- maximum number of retries reached
 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

char pm25_topic[100];
char pm100_topic[100];

int8_t read_i2c_registers(uint8_t i2c_addr, uint8_t register_id, uint8_t *data, uint16_t length)
{
    i2c_cmd_handle_t cmd;
    uint8_t ret;

    cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_addr << 1 | I2C_MASTER_WRITE, I2C_ACK);
    i2c_master_write_byte(cmd, register_id, I2C_ACK);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_addr << 1 | I2C_MASTER_READ, I2C_ACK);

    for (int i = 0; i < length - 1; i++)
    {
        i2c_master_read_byte(cmd, data + i, I2C_ACK);
    }
    i2c_master_read_byte(cmd, data + length - 1, I2C_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

int8_t write_i2c_registers(uint8_t i2c_addr, uint8_t register_id, uint8_t *data, uint16_t length)
{
    /*
    If length > 2, data must be preliminary interleaved with register addresses.
    */
    i2c_cmd_handle_t cmd;
    uint8_t ret;

    cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_addr << 1 | I2C_MASTER_WRITE, I2C_ACK);

    ESP_LOGV(LOG_TAG, "writing to register %i, number of bytes %i", register_id, length);

    i2c_master_write_byte(cmd, register_id, I2C_ACK);
    for (uint16_t i = 0; i < length; i++)
    {
        i2c_master_write_byte(cmd, *(data + i), i >= length ? I2C_NACK : I2C_ACK);
        ESP_LOGV(LOG_TAG, "%#2x", *(data + i));
    }
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

void publish_dust_data(const uint8_t *data, uint8_t length)
{

    if (length != 32)
    {
        return;
    }

    uint16_t pm25 = (data[12] << 8) + data[13];
    uint16_t pm100 = (data[14] << 8) + data[15];

    ESP_LOGD(LOG_TAG, "pm 2.5 atmo concentration is %05d", pm25);
    ESP_LOGD(LOG_TAG, "pm 10  atmo concentration is %05d", pm100);

    if (mqtt_connected)
    {
        int msg_id;
        char value[10];

        sprintf(value, "%d", pm25);
        msg_id = esp_mqtt_client_publish(mqtt_client, pm25_topic, value, 0, 0, 0);
        ESP_LOGI(LOG_TAG, "published pm25 value, msg_id=%d", msg_id);

        sprintf(value, "%d", pm100);
        msg_id = esp_mqtt_client_publish(mqtt_client, pm100_topic, value, 0, 0, 0);
        ESP_LOGI(LOG_TAG, "published pm100 value, msg_id=%d", msg_id);
    }
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
            publish_dust_data(b, result);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
    }
}

void delay_ms(uint32_t period_ms)
{
    vTaskDelay(period_ms / portTICK_PERIOD_MS);
}
void bmp280_task()
{

    struct bmp280_dev bmp;
    struct bmp280_config bmp_conf;

    bmp.delay_ms = delay_ms;
    bmp.dev_id = BMP280_I2C_ADDRESS;
    bmp.read = read_i2c_registers;
    bmp.write = write_i2c_registers;

    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 10000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_config);
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    ESP_LOGI(LOG_TAG, "i2c_0 initialized");

    int8_t res;

    if (bmp280_init(&bmp) != 0)
    {
        ESP_LOGE(LOG_TAG, "unable to initialize bmp280, panic");
        return;
    };

    if (bmp280_get_config(&bmp_conf, &bmp) != 0)
    {
        ESP_LOGE(LOG_TAG, "unable to get config of bmp280, panic");
        return;
    }

    bmp_conf.filter = BMP280_FILTER_COEFF_16;
    bmp_conf.os_pres = BMP280_OS_16X;
    bmp_conf.os_temp = BMP280_OS_4X;
    bmp_conf.odr = BMP280_ODR_1000_MS;

    if (bmp280_set_config(&bmp_conf, &bmp) != 0)
    {
        ESP_LOGE(LOG_TAG, "unable to set configuration for bmp280, panic");
        return;
    }

    if (bmp280_set_power_mode(BMP280_NORMAL_MODE, &bmp))
    {
        ESP_LOGE(LOG_TAG, "unable to set power mode for bmp280, panic");
        return;
    }

    struct bmp280_uncomp_data ucomp_data;
    double pres;
    double temp;

    for (;;)
    {
        if (bmp280_get_uncomp_data(&ucomp_data, &bmp) != 0)
        {
            ESP_LOGE(LOG_TAG, "unable to read raw data, panic");
            return;
        }

        bmp280_get_comp_temp_double(&temp, ucomp_data.uncomp_temp, &bmp);

        bmp280_get_comp_pres_double(&pres, ucomp_data.uncomp_press, &bmp);

        ESP_LOGI(LOG_TAG, "Tuncomp: %i", ucomp_data.uncomp_temp);
        ESP_LOGI(LOG_TAG, "T float: %f", temp);

        ESP_LOGI(LOG_TAG, "Puncomp: %i", ucomp_data.uncomp_press);
        ESP_LOGI(LOG_TAG, "P float: %f", pres);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
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

char *get_uniq_id()
{
    return "dust";
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(LOG_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = 1;
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = 0;
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(LOG_TAG, "Other event id:%d", event->event_id);
        break;
    }
    return;
}

void start_mqtt_client()
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER_URL,
        .username = MQTT_LOGIN,
        .password = MQTT_PASSWORD,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}

void app_main()
{

    esp_log_level_set("*", ESP_LOG_INFO);
    /*
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    esp_log_level_set("bmp280", ESP_LOG_VERBOSE);
    */
    esp_log_level_set(LOG_TAG, ESP_LOG_DEBUG);

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

    sprintf(pm25_topic, "%s%s%s", MQTT_TOPIC_PREFIX, get_uniq_id(), MQTT_TOPIC_PM25);
    sprintf(pm100_topic, "%s%s%s", MQTT_TOPIC_PREFIX, get_uniq_id(), MQTT_TOPIC_PM100);

    start_mqtt_client();

    //    xTaskCreate(dust_sensor_task, "dust_sensor_task", 4096, NULL, 10, NULL);
    xTaskCreate(bmp280_task, "bmp280_sensor_task", 4096, NULL, 10, NULL);
}
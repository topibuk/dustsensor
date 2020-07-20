#include <stdio.h>
#include <string.h>

#include "dust_sensor.h"

#include "esp_heap_task_info.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_event.h"

#define LOG_TAG "TASK:main"

EventGroupHandle_t eg_app_status;

char *get_uniq_id()
{
    return "dust";
}

/*
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
*/

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    esp_log_level_set("dust", ESP_LOG_VERBOSE);
    //    esp_log_level_set("bmp280", ESP_LOG_VERBOSE);
    esp_log_level_set(LOG_TAG, ESP_LOG_DEBUG);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    eg_app_status = xEventGroupCreate();
    xEventGroupClearBits(eg_app_status, 0xff);

    //    start_network();

    /*
    sprintf(pm25_topic, "%s%s%s", MQTT_TOPIC_PREFIX, get_uniq_id(), MQTT_TOPIC_PM25);
    sprintf(pm100_topic, "%s%s%s", MQTT_TOPIC_PREFIX, get_uniq_id(), MQTT_TOPIC_PM100);
    */

    //    start_mqtt_client();

    /*
    dust_values.lock = xSemaphoreCreateBinary();
    co2_values.lock = xSemaphoreCreateBinary();
    bmp_values.lock = xSemaphoreCreateBinary();

    xSemaphoreGive(dust_values.lock);
    xSemaphoreGive(co2_values.lock);
    xSemaphoreGive(bmp_values.lock);

    xTaskCreate(dust_sensor_task, "dust_sensor_task", 4096, NULL, 10, NULL);
    xTaskCreate(co2_sensor_task, "co2_sensor_task", 4096, NULL, 10, NULL);
    xTaskCreate(bmp280_task, "bmp280_sensor_task", 4096, NULL, 10, NULL);
    */

    xTaskCreate(network_task, "network_sensor_task", 4096, NULL, 10, NULL);
}
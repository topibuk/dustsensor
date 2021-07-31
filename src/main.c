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

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    esp_log_level_set("TASK: dust", ESP_LOG_INFO);
    esp_log_level_set("TASK: mqtt", ESP_LOG_VERBOSE);
    esp_log_level_set("TASK: pressure", ESP_LOG_INFO);
    esp_log_level_set(LOG_TAG, ESP_LOG_DEBUG);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    eg_app_status = xEventGroupCreate();
    xEventGroupClearBits(eg_app_status, 0xff);

    dust_values.lock = xSemaphoreCreateBinary();
    co2_values.lock = xSemaphoreCreateBinary();
    bmp_values.lock = xSemaphoreCreateBinary();

    xSemaphoreGive(dust_values.lock);
    xSemaphoreGive(co2_values.lock);
    xSemaphoreGive(bmp_values.lock);

    xTaskCreate(dust_sensor_task, "dust_sensor_task", 4096, NULL, 10, NULL);
    xTaskCreate(co2_sensor_task, "co2_sensor_task", 4096, NULL, 10, NULL);
    xTaskCreate(bmp_task, "bmp280_sensor_task", 4096, NULL, 10, NULL);

    xTaskCreate(network_task, "network_task", 4096, NULL, 10, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 10, NULL);
}
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "TASK: mqtt"

#include "dust_sensor.h"

#include "mqtt_client.h"

esp_mqtt_client_handle_t mqtt_client;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(LOG_TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(eg_app_status, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(eg_app_status, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(LOG_TAG, "MQTT_EVENT_ERROR");
        xEventGroupSetBits(eg_app_status, MQTT_MUST_DISCONNECT_BIT);
        break;
    default:
        ESP_LOGI(LOG_TAG, "Other event id:%d", event->event_id);
        break;
    }
    return;
}

void start_mqtt_client()
{
    ESP_LOGV(LOG_TAG, "starting mqtt client");

    esp_mqtt_client_start(mqtt_client);
}

void stop_mqtt_client()
{
    ESP_LOGV(LOG_TAG, "stopping mqtt client");
    if (!mqtt_client)
        return;
    esp_mqtt_client_stop(mqtt_client);
}

void sendMQTTupdate()
{
    char value[64];
    char topic[128];
    int msg_id;

    ESP_LOGD(LOG_TAG, "sending updates via mqtt");

    xSemaphoreTake(dust_values.lock, SEMAPHORE_TIMEOUT / portTICK_PERIOD_MS);
    sprintf(value, "%d", dust_values.pm25);
    xSemaphoreGive(dust_values.lock);
    sprintf(topic, "%s/%s", MQTT_TOPIC_PREFIX, MQTT_TOPIC_PM25);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 0, 0);
    ESP_LOGD(LOG_TAG, "published pm25 value=%s, msg_id=%d", value, msg_id);

    xSemaphoreTake(dust_values.lock, SEMAPHORE_TIMEOUT / portTICK_PERIOD_MS);
    sprintf(value, "%d", dust_values.pm100);
    xSemaphoreGive(dust_values.lock);
    sprintf(topic, "%s/%s", MQTT_TOPIC_PREFIX, MQTT_TOPIC_PM100);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 0, 0);
    ESP_LOGD(LOG_TAG, "published pm100 value=%s, msg_id=%d", value, msg_id);

    xSemaphoreTake(co2_values.lock, SEMAPHORE_TIMEOUT / portTICK_PERIOD_MS);
    sprintf(value, "%d", co2_values.ppm);
    xSemaphoreGive(co2_values.lock);
    sprintf(topic, "%s/%s", MQTT_TOPIC_PREFIX, MQTT_TOPIC_CO2);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 0, 0);
    ESP_LOGD(LOG_TAG, "published co2 value=%s, msg_id=%d", value, msg_id);

    xSemaphoreTake(bmp_values.lock, SEMAPHORE_TIMEOUT / portTICK_PERIOD_MS);
    sprintf(value, "%0.0f", bmp_values.pres / 133.322);
    xSemaphoreGive(bmp_values.lock);
    sprintf(topic, "%s/%s", MQTT_TOPIC_PREFIX, MQTT_TOPIC_PRES);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 0, 0);
    ESP_LOGD(LOG_TAG, "published pressure value=%s, msg_id=%d", value, msg_id);

    xSemaphoreTake(bmp_values.lock, SEMAPHORE_TIMEOUT / portTICK_PERIOD_MS);
    sprintf(value, "%0.1f", bmp_values.temp);
    xSemaphoreGive(bmp_values.lock);
    sprintf(topic, "%s/%s", MQTT_TOPIC_PREFIX, MQTT_TOPIC_TEMP);
    msg_id = esp_mqtt_client_publish(mqtt_client, topic, value, 0, 0, 0);
    ESP_LOGD(LOG_TAG, "published temperature value=%s, msg_id=%d", value, msg_id);
}

#define statusMQTT_MUST_DISCONNECT(a) (a & MQTT_MUST_DISCONNECT_BIT)
#define statusWIFI_CONNECTED(a) (a & WIFI_CONNECTED_BIT)
#define statusMQTT_CONNECTED(a) (a & MQTT_CONNECTED_BIT)

void mqtt_task()
{
    EventBits_t bits;

    ESP_LOGI(LOG_TAG, "task started");

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER_URL,
        .username = MQTT_LOGIN,
        .password = MQTT_PASSWORD,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);

    for (;;)
    {

        ESP_LOGD(LOG_TAG, "cycle");

        bits = xEventGroupWaitBits(eg_app_status, MQTT_MUST_DISCONNECT_BIT,
                                   pdFALSE,
                                   pdFALSE,
                                   MQTT_DELAY / portTICK_PERIOD_MS);

        ESP_LOGD(LOG_TAG, "eg_ap_status event group value: %i", bits);

        if ((statusMQTT_MUST_DISCONNECT(bits) && statusMQTT_CONNECTED(bits)) ||
            (!statusWIFI_CONNECTED(bits) && statusMQTT_CONNECTED(bits)))
        {
            ESP_LOGI(LOG_TAG, "stopping MQTT client");
            stop_mqtt_client();
            xEventGroupClearBits(eg_app_status, MQTT_CONNECTED_BIT | MQTT_MUST_DISCONNECT_BIT);
        }
        else if (statusWIFI_CONNECTED(bits) && !statusMQTT_CONNECTED(bits))
        {
            ESP_LOGI(LOG_TAG, "strating MQTT client");
            // FIXME: do not start mqtt client if "startup" procedure in progress
            start_mqtt_client();
            xEventGroupSetBits(eg_app_status, MQTT_CONNECTED_BIT);
        }
        else if (statusMQTT_CONNECTED(bits) && statusWIFI_CONNECTED(bits) &&
                 !statusMQTT_MUST_DISCONNECT(bits))
        {
            sendMQTTupdate();
        }
        xEventGroupClearBits(eg_app_status, MQTT_MUST_DISCONNECT_BIT);
    }
}
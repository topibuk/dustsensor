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

void stop_mqtt_client()
{
    if (!mqtt_client)
        return;
    esp_mqtt_client_stop(mqtt_client);
}

void mqtt_task()
{
    for (;;)
    {

        EventBits_t bits = xEventGroupGetBits(eg_app_status);
        if (bits && MQTT_CONNECTED_BIT & bits && WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(LOG_TAG, "sending updates via mqtt");
        }
        else if (bits && WIFI_CONNECTED_BIT)
        {
            // FIXME: do not start mqtt client if "startup" procedure in progress
            start_mqtt_client();
        }
        else if (bits && MQTT_CONNECTED_BIT)
        {
            stop_mqtt_client();
        }
        vTaskDelay(MQTT_DELAY / portTICK_PERIOD_MS);
    }
}
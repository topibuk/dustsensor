#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "TASK: wifi"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

#include "dust_sensor.h"

#define WIFI_CONNECT_MAXIMUM_RETRY 100

uint8_t wifi_connect_retry_counter;

uint8_t wifi_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            wifi_connect_retry_counter++;
            ESP_LOGI(LOG_TAG, "connecting to the AP");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits(eg_app_status, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(eg_app_status, MQTT_MUST_DISCONNECT_BIT);

            if (wifi_connect_retry_counter < WIFI_CONNECT_MAXIMUM_RETRY)
            {
                ESP_LOGI(LOG_TAG, "disconnected from AP");
                esp_wifi_connect();
                wifi_connect_retry_counter++;
                ESP_LOGI(LOG_TAG, "reconnecting");
            }
            else
            {
                xEventGroupSetBits(eg_app_status, WIFI_FAIL_BIT);
            }
            ESP_LOGI(LOG_TAG, "connect to the AP fail");
            break;
        default:
            ESP_LOGI(LOG_TAG, "unhandled WIFI_EVENT");
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(LOG_TAG, "got ip:%s",
                     ip4addr_ntoa((ip4_addr_t *)&event->ip_info.ip));
            wifi_connect_retry_counter = 0;
            ESP_LOGD(LOG_TAG, "set bits 2");
            if (!eg_app_status)
            {
                ESP_LOGE(LOG_TAG, "esp_app_status is NULL!");
                return;
            }
            xEventGroupSetBits(eg_app_status, WIFI_CONNECTED_BIT);
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(LOG_TAG, "ip lost");
            break;
        default:
            ESP_LOGI(LOG_TAG, "unhandled IP_EVENT");
        }
    }
}

void network_task()
{

    //Initialize NVS for internal use of WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(LOG_TAG, "starting WiFi in station mode");

    tcpip_adapter_init();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    //    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(LOG_TAG, "wifi in station mode started");

    for (;;)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

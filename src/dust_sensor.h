#ifndef _DUST_SENSOR_H
#define _DUST_SENSOR_H

#include "stdatomic.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "pms7003.h"
#include "bmp.h"
#include "mhz19.h"

#include "secrets.h"

/* FreeRTOS event group to signal when we are connected*/
extern EventGroupHandle_t eg_app_status;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MQTT_CONNECTED_BIT BIT2
#define MQTT_MUST_DISCONNECT_BIT BIT3
#define MQTT_DELAY 10000 //microseconds

#define SEMAPHORE_TIMEOUT 2000 // microseconds

#define DUST_PIN_RX GPIO_NUM_16
#define DUST_PIN_TX GPIO_NUM_17

#define DUST_TASK_DELAY 10000 //microseconds

#define BMP_SDA_PIN GPIO_NUM_33
#define BMP_SCL_PIN GPIO_NUM_32

#define BMP_TASK_DELAY 10000 //microseconds

#define CO2_PIN_RX GPIO_NUM_21
#define CO2_PIN_TX GPIO_NUM_19

#define CO2_TASK_DELAY 10000 //microseconds

struct dust_values_s
{
    uint16_t pm25;
    uint16_t pm100;
    uint8_t updated;
    SemaphoreHandle_t lock;
} dust_values;

void co2_sensor_task();
void dust_sensor_task();
void bmp_task();
void network_task();
void mqtt_task();

void start_network();

struct co2_values_s
{
    uint16_t ppm;
    uint8_t updated;
    SemaphoreHandle_t lock;
} co2_values;

struct bmp_values_s
{
    double pres;
    double temp;
    uint8_t updated;
    SemaphoreHandle_t lock;
} bmp_values;

#endif // _DUST_SENSOR_H
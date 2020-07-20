#ifndef _DUST_SENSOR_H
#define _DUST_SENSOR_H

#include "stdatomic.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "secrets.h"

#define DUST_PIN_RX GPIO_NUM_16
#define DUST_PIN_TX GPIO_NUM_17
#define DUST_TASK_DELAY 10000 //microseconds

static atomic_char mqtt_connected = 0;

void dust_sensor_task();
void co2_sensor_task();
void bmp280_task();

void start_network();

struct dust_values_s
{
    uint16_t pm25;
    uint16_t pm100;
    uint8_t updated;
    SemaphoreHandle_t lock;
} dust_values;

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
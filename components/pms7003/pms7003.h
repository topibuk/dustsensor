#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define DUST_PIN_RX GPIO_NUM_16
#define DUST_PIN_TX GPIO_NUM_17
#define DUST_TASK_DELAY 10000 //microseconds

void dust_sensor_task();

struct dust_values_s
{
	uint16_t pm25;
	uint16_t pm100;
	uint8_t updated;
	SemaphoreHandle_t lock;
} dust_values;

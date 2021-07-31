#ifndef _PMS7003_H
#define _PMS7003_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#define PMS_MAX_FAILS 100

typedef struct
{
	uint16_t pm25;
	uint16_t pm100;
} pms_values_t;

int pms_set_passive_mode();

int pms_init(int pin_tx, int pin_rx, int uart_num);

int pms_fill_values(pms_values_t *values);

#endif // _PMS7003_H
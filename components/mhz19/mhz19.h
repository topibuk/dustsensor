#include "driver/gpio.h"
#include "driver/uart.h"

typedef struct
{
	uint16_t ppm;

} mhz19_values_t;

int mhz19_init(int pin_tx, int pin_rx, int uart_num);

int mhz19_fill_values(mhz19_values_t *values);
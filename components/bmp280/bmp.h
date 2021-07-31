#ifndef _BMP_H
#define _BMP_H

#include "driver/i2c.h"

#include "bmp280.h"

struct bmp280_dev bmp;

typedef struct
{
	double temp;
	double pres;
} bmp_values_t;

int bmp_init(int sda_pin, int scl_pin, int i2c_num);

int bmp_fill_values(bmp_values_t *values);

#endif // _BMP_H
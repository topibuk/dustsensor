#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "MH-Z19:"

#include "mhz19.h"

#define CO2_MAX_FAILS 20

#define BUF_SIZE UART_FIFO_LEN * 2

int _uart_num;

static char cmd_co2_read[] = {
    0xFF,
    0x01,
    0x86,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x79,
};

uint8_t mhz_checksum(uint8_t *packet)
{
	uint8_t i;
	unsigned char checksum = 0;
	for (i = 1; i < 8; i++)
	{
		checksum += packet[i];
	}
	checksum = 0xff - checksum;
	checksum += 1;
	return checksum;
}

int mhz19_init(int pin_tx, int pin_rx, int uart_num)
{
	uart_config_t co2_config = {

	    .baud_rate = 9600,
	    .data_bits = UART_DATA_8_BITS,
	    .parity = UART_PARITY_DISABLE,
	    .stop_bits = UART_STOP_BITS_1,
	    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE

	};

	_uart_num = uart_num;

	uart_param_config(_uart_num, &co2_config);
	uart_set_pin(_uart_num, pin_tx, pin_rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_driver_install(_uart_num, BUF_SIZE, BUF_SIZE, 0, NULL, 0);

	return ESP_OK;
}

int mhz19_fill_values(mhz19_values_t *values)
{
	int8_t res;
	uint8_t checksum;

	uint8_t data[9];
	uint8_t count = 0;

	uint8_t fails_count = 0;

	uart_flush(_uart_num);
	res = uart_write_bytes(_uart_num, (const char *)cmd_co2_read, sizeof(cmd_co2_read));

	if (res < 0)
	{
		ESP_LOGE(LOG_TAG, "can't write to co2 sensor, panic");
		return ESP_FAIL;
	}

	while (count < sizeof(data))
	{
		count = uart_read_bytes(_uart_num, data + count, sizeof(data) - count, 200 / portTICK_PERIOD_MS);
		if (count <= 0)
		{
			fails_count++;
		}

		if (fails_count > CO2_MAX_FAILS)
		{
			ESP_LOGW(LOG_TAG, "unable to read from co2 sensor");
			break;
		}
	}

	checksum = mhz_checksum(data);

	ESP_LOGV(LOG_TAG, "received_checksum is 0x%x", data[8]);
	ESP_LOGV(LOG_TAG, "Calculated CRC 0x%x", checksum);

	if (checksum == data[8])
	{
		values->ppm = (uint16_t)((uint16_t)data[2] << 8 | (uint16_t)data[3]);
	}
	else
	{
		values->ppm = 0;
		ESP_LOGW(LOG_TAG, "wrong checksum");
	}

	return ESP_OK;
}
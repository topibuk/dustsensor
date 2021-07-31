#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "PMS7003:"

#include "pms7003.h"

#define DUST_RX_BUF_SIZE UART_FIFO_LEN * 2
#define DUST_TX_BUF_SIZE 0

static char cmd_pms_set_passive_mode[] = {0x42,
					  0x4D,
					  0xE1,
					  0x00,
					  0x00,
					  0x01,
					  0x70};

// 0x42 + 0x4D + 0xE1 + 0x00 + 0x00 + 0x01 = 0x170 = 0x1 << 8 + 0x70

static char cmd_pms_read[] = {0x42,
			      0x4D,
			      0xE2,
			      0x00,
			      0x00,
			      0x01,
			      0x71};

// 0x42 + 0x4D + 0xE2 + 0x00 + 0x00 + 0x01 = 0x171 = 0x1 << 8 + 0x71

uint16_t pms_checksum(uint8_t *buffer, uint8_t length)
{
	uint8_t i;
	uint16_t result = 0;

	for (i = 0; i < length; i++)
	{
		result += buffer[i];
	}

	return result;
}

int pms_init(int pin_tx, int pin_rx, int uart_num)
{
	uart_config_t dust_config = {

	    .baud_rate = 9600,
	    .data_bits = UART_DATA_8_BITS,
	    .parity = UART_PARITY_DISABLE,
	    .stop_bits = UART_STOP_BITS_1,
	    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE

	};

	uart_param_config(uart_num, &dust_config);
	uart_set_pin(uart_num, pin_tx, pin_rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

	uart_driver_install(uart_num, DUST_RX_BUF_SIZE, DUST_TX_BUF_SIZE, 0, NULL, 0);

	return ESP_OK;
}

int pms_set_passive_mode()
{
	uart_write_bytes(UART_NUM_2, (const char *)cmd_pms_set_passive_mode, sizeof(cmd_pms_set_passive_mode));

	return ESP_OK;
}

int pms_fill_values(pms_values_t *values)
{
	int8_t res;
	uint8_t data[32];
	uint8_t count = 0;

	uint8_t fails_count = 0;

	uint16_t received_checksum;
	uint16_t calculated_checksum;

	uart_flush(UART_NUM_2);
	res = uart_write_bytes(UART_NUM_2, (const char *)cmd_pms_read, sizeof(cmd_pms_read));

	if (res < 0)
	{
		ESP_LOGE(LOG_TAG, "can't write to dust sensor, panic");
		return ESP_FAIL;
	}

	count = 0;

	while (count < sizeof(data))
	{
		count = uart_read_bytes(UART_NUM_2, data + count, sizeof(data) - count, 200 / portTICK_PERIOD_MS);
		ESP_LOGV(LOG_TAG, "read %i bytes", count);
		if (count <= 0)
		{
			fails_count++;
		}

		if (fails_count > PMS_MAX_FAILS)
		{
			ESP_LOGW(LOG_TAG, "unable to read from dust sensor");
			break;
		}
	}

	received_checksum = (data[30] << 8) + data[31];
	calculated_checksum = pms_checksum(data, 30);

	ESP_LOGV(LOG_TAG, "received_checksum is 0x%x", received_checksum);
	ESP_LOGV(LOG_TAG, "Calculated CRC 0x%x", calculated_checksum);

	if (received_checksum == calculated_checksum)
	{

		values->pm25 = (data[12] << 8) + data[13];
		values->pm100 = (data[14] << 8) + data[15];
	}
	else
	{
		values->pm25 = 0;
		values->pm100 = 0;
		ESP_LOGW(LOG_TAG, "wrong checksum");
	}

	return ESP_OK;
}
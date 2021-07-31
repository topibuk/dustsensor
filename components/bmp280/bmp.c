#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "BMP280 I2C:"

#include "bmp.h"

#define BMP280_I2C_ADDRESS 0x76

#define I2C_ACK 0x0
#define I2C_NACK 0x1

int8_t read_i2c_registers(uint8_t i2c_addr, uint8_t register_id, uint8_t *data, uint16_t length)
{
	i2c_cmd_handle_t cmd;
	uint8_t ret;

	cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, i2c_addr << 1 | I2C_MASTER_WRITE, I2C_ACK);
	i2c_master_write_byte(cmd, register_id, I2C_ACK);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, i2c_addr << 1 | I2C_MASTER_READ, I2C_ACK);

	for (int i = 0; i < length - 1; i++)
	{
		i2c_master_read_byte(cmd, data + i, I2C_ACK);
	}
	i2c_master_read_byte(cmd, data + length - 1, I2C_NACK);
	i2c_master_stop(cmd);

	ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	return ret;
}

int8_t write_i2c_registers(uint8_t i2c_addr, uint8_t register_id, uint8_t *data, uint16_t length)
{
	/*
	If length > 2, data must be preliminary interleaved with register addresses.
	*/

	i2c_cmd_handle_t cmd;
	uint8_t ret;

	cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, i2c_addr << 1 | I2C_MASTER_WRITE, I2C_ACK);

	ESP_LOGV(LOG_TAG, "writing to register %i, number of bytes %i", register_id, length);

	i2c_master_write_byte(cmd, register_id, I2C_ACK);
	for (uint16_t i = 0; i < length; i++)
	{
		i2c_master_write_byte(cmd, *(data + i), i >= length ? I2C_NACK : I2C_ACK);
		ESP_LOGV(LOG_TAG, "%#2x", *(data + i));
	}
	i2c_master_stop(cmd);

	ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	return ret;
}

void delay_ms(uint32_t period_ms)
{
	vTaskDelay(period_ms / portTICK_PERIOD_MS);
}

int bmp_init(int sda_pin, int scl_pin, int i2c_num)
{
	struct bmp280_config bmp_conf;

	bmp.delay_ms = delay_ms;
	bmp.dev_id = BMP280_I2C_ADDRESS;
	bmp.read = read_i2c_registers;
	bmp.write = write_i2c_registers;

	i2c_config_t i2c_config = {
	    .mode = I2C_MODE_MASTER,
	    .sda_io_num = sda_pin,
	    .scl_io_num = scl_pin,
	    .sda_pullup_en = GPIO_PULLUP_ENABLE,
	    .scl_pullup_en = GPIO_PULLUP_ENABLE,
	    .master.clk_speed = 10000,
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	ESP_ERROR_CHECK(i2c_driver_install(i2c_num, I2C_MODE_MASTER, 0, 0, 0));
	ESP_LOGI(LOG_TAG, "i2c_0 initialized");

	if (bmp280_init(&bmp) != 0)
	{
		ESP_LOGE(LOG_TAG, "unable to initialize bmp280, panic");
		return ESP_FAIL;
	};

	if (bmp280_get_config(&bmp_conf, &bmp) != 0)
	{
		ESP_LOGE(LOG_TAG, "unable to get config of bmp280, panic");
		return ESP_FAIL;
	}

	bmp_conf.filter = BMP280_FILTER_COEFF_16;
	bmp_conf.os_pres = BMP280_OS_16X;
	bmp_conf.os_temp = BMP280_OS_4X;
	bmp_conf.odr = BMP280_ODR_1000_MS;

	if (bmp280_set_config(&bmp_conf, &bmp) != 0)
	{
		ESP_LOGE(LOG_TAG, "unable to set configuration for bmp280, panic");
		return ESP_FAIL;
	}

	if (bmp280_set_power_mode(BMP280_NORMAL_MODE, &bmp))
	{
		ESP_LOGE(LOG_TAG, "unable to set power mode for bmp280, panic");
		return ESP_FAIL;
	}

	return ESP_OK;
}

int bmp_fill_values(bmp_values_t *values)
{
	struct bmp280_uncomp_data ucomp_data;

	if (bmp280_get_uncomp_data(&ucomp_data, &bmp) != 0)
	{
		ESP_LOGE(LOG_TAG, "unable to read raw data, panic");
		return ESP_FAIL;
	}

	bmp280_get_comp_temp_double(&(values->temp), ucomp_data.uncomp_temp, &bmp);

	bmp280_get_comp_pres_double(&(values->pres), ucomp_data.uncomp_press, &bmp);

	return ESP_OK;
}
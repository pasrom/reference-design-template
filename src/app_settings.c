/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/settings.h>
#include "main.h"
#include "app_settings.h"

static int32_t _loop_delay_s = 60;
static int32_t _co2_offset = 0;
static int32_t _temperature_offset = 0.0;
static int32_t _humidity_offset = 0.0;
#define LOOP_DELAY_S_MAX 43200
#define LOOP_DELAY_S_MIN 0
#define CO2_OFFSET_S_MAX 1000
#define CO2_OFFSET_S_MIN -1000

int32_t get_loop_delay_s(void)
{
	return _loop_delay_s;
}

int32_t get_co2_offset_ppm(void)
{
	return _co2_offset;
}

float get_temperature_offset_gc(void)
{
	return _temperature_offset;
}

float get_humidity_offset_p(void)
{
	return _humidity_offset;
}

static enum golioth_settings_status on_loop_delay_setting(int32_t new_value, void *arg)
{
	_loop_delay_s = new_value;
	LOG_INF("Set loop delay to %i seconds", new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_co2_offset_setting(int32_t new_value, void *arg)
{
	_co2_offset = new_value;
	LOG_INF("Set co2 offset to %i ppm", new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_temperature_offset_setting(float new_value, void *arg)
{
	_temperature_offset = new_value;
	LOG_INF("Set temperature offset to %.02f °C", (double)new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_humidity_offset_setting(float new_value, void *arg)
{
	_humidity_offset = new_value;
	LOG_INF("Set humidity offset to %.02f %%", (double)new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

int app_settings_register(struct golioth_client *client)
{
	struct golioth_settings *settings = golioth_settings_init(client);

	int err = golioth_settings_register_int_with_range(settings, "LOOP_DELAY_S",
							   LOOP_DELAY_S_MIN, LOOP_DELAY_S_MAX,
							   on_loop_delay_setting, NULL);

	err |= golioth_settings_register_int_with_range(settings, "CO2_OFFSET_PPM",
							CO2_OFFSET_S_MIN, CO2_OFFSET_S_MAX,
							on_co2_offset_setting, NULL);

	err |= golioth_settings_register_float(settings, "TEMPERATURE_OFFSET_DC",
					       on_temperature_offset_setting, NULL);

	err |= golioth_settings_register_float(settings, "HUMIDITY_OFFSET_G",
					       on_humidity_offset_setting, NULL);

	if (err) {
		LOG_ERR("Failed to register settings callback: %d", err);
	}

	return err;
}

/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_sensors, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <sensor/scd30/scd30.h>

#include "app_sensors.h"
#ifdef CONFIG_SPS30
#include "sensors.h"
#endif
#include "app_settings.h"

#ifdef CONFIG_LIB_OSTENTUS
#include <libostentus.h>
#endif
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

static struct golioth_client *client;
/* Add Sensor structs here */

/* Formatting string for sending sensor JSON to Golioth */
#ifdef CONFIG_SPS30
#define JSON_FMT                                                                                   \
	"{\"counter\":%d,\"co2\":%.2f,\"temperature\":%.2f,\"humidity\":%.2f,\"mc_1p0\":%f,\"mc_"  \
	"2p5\":%f,\"mc_4p0\":%f,\"mc_10p0\":%f,\"nc_0p5\":%f,\"nc_1p0\":%f,\"nc_2p5\":%f,\"nc_"    \
	"4p0\":%f,\"nc_10p0\":%f,\"tps\":%f}"
static struct sps30_sensor_measurement sps30_sm;
#else /* CONFIG_SPS30 */
#define JSON_FMT "{\"counter\":%d,\"co2\":%.2f,\"temperature\":%.2f,\"humidity\":%.2f}"
#endif /* CONFIG_SPS30 */

#define SCD30_SAMPLE_TIME_SECONDS 5U

static const struct device *dev = DEVICE_DT_GET_ANY(sensirion_scd30);

/* Callback for LightDB Stream */

static void async_error_handler(struct golioth_client *client,
				const struct golioth_response *response, const char *path,
				void *arg)
{
	if (response->status != GOLIOTH_OK) {
		LOG_ERR("Async task failed: %d", response->status);
		return;
	}
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_sensors_read_and_steam(void)
{
	int err;
	char json_buf[256];
	struct sensor_value co2_concentration;
	struct sensor_value temperature;
	struct sensor_value humidity;

	int rc = sensor_sample_fetch(dev);
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_CO2, &co2_concentration);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);
	}

	if (rc == -ENODATA) {
		LOG_INF("%s: no new measurment yet.", dev->name);
		LOG_INF("Waiting for 1 second and retrying...");
	} else if (rc != 0) {
		LOG_INF("%s channel get: failed: %d", dev->name, rc);
	}

	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (
		read_and_report_battery(client);
		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			slide_set(BATTERY_V, get_batt_v_str(), strlen(get_batt_v_str()));
			slide_set(BATTERY_LVL, get_batt_lvl_str(), strlen(get_batt_lvl_str()));
		));
	));

	/* For this demo, we just send counter data to Golioth */
	static uint8_t counter;
	float co2_value = sensor_value_to_float(&co2_concentration) + get_co2_offset_ppm();
	float temperature_value = sensor_value_to_float(&temperature) + get_temperature_offset_gc();
	float humidity_value = sensor_value_to_float(&humidity) + get_humidity_offset_p();

#ifdef CONFIG_SPS30
	/* Read the PM sensor */
	err = sps30_sensor_read(&sps30_sm);
	if (err) {
		LOG_ERR("Failed to read from PM Sensor SPS30: %d", err);
		// return;
	} else {
		LOG_DBG("sps30: "
			"PM1.0=%f μg/m³, PM2.5=%f μg/m³, "
			"PM4.0=%f μg/m³, PM10.0=%f μg/m³, "
			"NC0.5=%f #/cm³, NC1.0=%f #/cm³, "
			"NC2.5=%f #/cm³, NC4.0=%f #/cm³, "
			"NC10.0=%f #/cm³, Typical Particle Size=%f μm",
			sensor_value_to_float(&sps30_sm.mc_1p0),
			sensor_value_to_float(&sps30_sm.mc_2p5),
			sensor_value_to_float(&sps30_sm.mc_4p0),
			sensor_value_to_float(&sps30_sm.mc_10p0),
			sensor_value_to_float(&sps30_sm.nc_0p5),
			sensor_value_to_float(&sps30_sm.nc_1p0),
			sensor_value_to_float(&sps30_sm.nc_2p5),
			sensor_value_to_float(&sps30_sm.nc_4p0),
			sensor_value_to_float(&sps30_sm.nc_10p0),
			sensor_value_to_float(&sps30_sm.typical_particle_size));
	}

	snprintf(json_buf, sizeof(json_buf), JSON_FMT, counter, co2_value, temperature_value,
		 humidity_value, sensor_value_to_float(&sps30_sm.mc_1p0),
		 sensor_value_to_float(&sps30_sm.mc_2p5), sensor_value_to_float(&sps30_sm.mc_4p0),
		 sensor_value_to_float(&sps30_sm.mc_10p0), sensor_value_to_float(&sps30_sm.nc_0p5),
		 sensor_value_to_float(&sps30_sm.nc_1p0), sensor_value_to_float(&sps30_sm.nc_2p5),
		 sensor_value_to_float(&sps30_sm.nc_4p0), sensor_value_to_float(&sps30_sm.nc_10p0),
		 sensor_value_to_float(&sps30_sm.typical_particle_size));
#else
	/* Send sensor data to Golioth */
	/* For this demo we just fake it */
	snprintf(json_buf, sizeof(json_buf), JSON_FMT, counter, co2_value, temperature_value,
		 humidity_value);
#endif
	LOG_DBG("%s", json_buf);

	err = golioth_stream_set_async(client, "sensor", GOLIOTH_CONTENT_TYPE_JSON, json_buf,
				       strlen(json_buf), async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	IF_ENABLED(CONFIG_LIB_OSTENTUS, (
		/* Update slide values on Ostentus
		 *  -values should be sent as strings
		 *  -use the enum from app_sensors.h for slide key values
		 */
		snprintk(json_buf, sizeof(json_buf), "%d", counter);
		slide_set(UP_COUNTER, json_buf, strlen(json_buf));
		snprintk(json_buf, sizeof(json_buf), "%d", 255 - counter);
		slide_set(DN_COUNTER, json_buf, strlen(json_buf));
	));
	++counter;
}

void app_sensors_init(struct golioth_client *work_client)
{
	client = work_client;
	struct sensor_value sample_period = {
		.val1 = SCD30_SAMPLE_TIME_SECONDS,
	};

	if (!device_is_ready(dev)) {
		LOG_ERR("Could not get SCD30 device");
		return;
	}

	int rc = sensor_attr_set(dev, SENSOR_CHAN_ALL, SCD30_SENSOR_ATTR_SAMPLING_PERIOD,
				 &sample_period);
	if (rc != 0) {
		LOG_ERR("Failed to set sample period. (%d)", rc);
		return;
	}
}

void app_sensors_co2_calibrate(int32_t value)
{
	struct sensor_value calibration = {
		.val1 = value,
	};
	int err = sensor_attr_set(dev, SENSOR_CHAN_ALL, SCD30_SENSOR_SET_FORCED_RECALIBRATION,
				  &calibration);
	if (err != 0) {
		LOG_ERR("Failed to get calib target. (%d)", err);
		return;
	}
}

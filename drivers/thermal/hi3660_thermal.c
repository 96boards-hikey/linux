/*
 * Hi3660 thermal driver.
 *
 * Copyright (c) 2017 Hisilicon Limited.
 * Copyright (c) 2017 Linaro Limited.
 *
 * Author: Leo Yan <leo.yan@linaro.org>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/mailbox_client.h>

#include "thermal_core.h"

#define HISI_MAX_SENSORS		4

/* hi3660 Thermal Sensor Dev Structure */
struct hi3660_thermal_sensor {
	struct hi3660_thermal_data *thermal;
	struct thermal_zone_device *tzd;

	uint32_t id;

	unsigned int msg[8];
};

struct hi3660_thermal_data {
	struct mutex thermal_lock;    /* protects register data */
	struct platform_device *pdev;
	struct hi3660_thermal_sensor sensors[HISI_MAX_SENSORS];

	struct mbox_client cl;
	struct mbox_chan *mbox;
};


static int hi3660_thermal_get_temp(void *_sensor, int *temp)
{
	struct hi3660_thermal_sensor *sensor = _sensor;
	struct hi3660_thermal_data *data = sensor->thermal;
	int val;

	mutex_lock(&data->thermal_lock);

	sensor->msg[0] = 0x000E020B;
	sensor->msg[1] = (sensor->msg[1] & 0xFFFF0000) | sensor->id;

	mbox_send_message(data->mbox, sensor->msg);

	val = sensor->msg[1] >> 16;
	val = clamp_val(val, 116, 922);
	val = ((val - 116)* 1000 * 165) / (922 - 116) - 40000;
	*temp = val;

	mutex_unlock(&data->thermal_lock);
	return 0;
}

static struct thermal_zone_of_device_ops hi3660_of_thermal_ops = {
	.get_temp = hi3660_thermal_get_temp,
};

static int hi3660_thermal_register_sensor(struct platform_device *pdev,
		struct hi3660_thermal_data *data,
		struct hi3660_thermal_sensor *sensor,
		int index)
{
	int ret;

	sensor->id = index;
	sensor->thermal = data;

	sensor->tzd = thermal_zone_of_sensor_register(&pdev->dev, sensor->id,
				sensor, &hi3660_of_thermal_ops);
	if (IS_ERR(sensor->tzd)) {
		ret = PTR_ERR(sensor->tzd);
		sensor->tzd = NULL;
		dev_err(&pdev->dev, "failed to register sensor id %d: %d\n",
			sensor->id, ret);
		return ret;
	}

	return 0;
}

static void hi3660_thermal_toggle_sensor(struct hi3660_thermal_sensor *sensor,
				       bool on)
{
	struct thermal_zone_device *tzd = sensor->tzd;

	tzd->ops->set_mode(tzd,
		on ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED);
}

static int hi3660_thermal_probe(struct platform_device *pdev)
{
	struct hi3660_thermal_data *data;
	int ret = 0;
	int i;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->thermal_lock);
	data->pdev = pdev;

	platform_set_drvdata(pdev, data);

	/* Use mailbox client with blocking mode */
	data->cl.dev = &pdev->dev;
	data->cl.tx_done = NULL;
	data->cl.tx_block = true;
	data->cl.tx_tout = 500;
	data->cl.knows_txdone = false;

	/* Allocate mailbox channel */
	data->mbox = mbox_request_channel(&data->cl, 0);
	if (IS_ERR(data->mbox)) {
		dev_err(&pdev->dev, "failed get mailbox channel\n");
		return PTR_ERR(data->mbox);
	}

	for (i = 0; i < HISI_MAX_SENSORS; ++i) {
		ret = hi3660_thermal_register_sensor(pdev, data,
						     &data->sensors[i], i);
		if (ret)
			dev_err(&pdev->dev,
				"failed to register thermal sensor: %d\n", ret);
		else
			hi3660_thermal_toggle_sensor(&data->sensors[i], true);
	}

	dev_info(&pdev->dev, "Thermal Sensor Loaded\n");
	return 0;
}

static int hi3660_thermal_exit(struct platform_device *pdev)
{
	struct hi3660_thermal_data *data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < HISI_MAX_SENSORS; i++) {
		struct hi3660_thermal_sensor *sensor = &data->sensors[i];

		if (!sensor->tzd)
			continue;

		hi3660_thermal_toggle_sensor(sensor, false);
		thermal_zone_of_sensor_unregister(&pdev->dev, sensor->tzd);
	}

	mbox_free_channel(data->mbox);
	return 0;
}

static const struct of_device_id hi3660_thermal_id_table[] = {
	{ .compatible = "hisilicon,thermal-hi3660" },
	{}
};
MODULE_DEVICE_TABLE(of, hi3660_thermal_id_table);

static struct platform_driver hi3660_thermal_driver = {
	.probe = hi3660_thermal_probe,
	.remove = hi3660_thermal_exit,
	.driver = {
		.name = "hi3660_thermal",
		.of_match_table = hi3660_thermal_id_table,
	},
};

module_platform_driver(hi3660_thermal_driver);

MODULE_AUTHOR("Leo Yan <leo.yan@linaro.org>");
MODULE_DESCRIPTION("hi3660 thermal driver");
MODULE_LICENSE("GPL");

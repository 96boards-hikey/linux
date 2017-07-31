/*
 *  linux/drivers/thermal/hi3660_thermal.c
 *
 *  Copyright (c) 2017 Hisilicon Limited.
 *  Copyright (c) 2017 Linaro Limited.
 *
 *  Author: Tao Wang <kevin.wangtao@hisilicon.com>
 *  Author: Leo Yan <leo.yan@linaro.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "thermal_core.h"

#define HW_MAX_SENSORS			4
#define HISI_MAX_SENSORS		6
#define SENSOR_MAX			4
#define SENSOR_AVG			5

#define ADC_MIN		116
#define ADC_MAX		922

/* hi3660 Thermal Sensor Dev Structure */
struct hi3660_thermal_sensor {
	struct hi3660_thermal_data *thermal;
	struct thermal_zone_device *tzd;

	uint32_t id;
};

struct hi3660_thermal_data {
	struct platform_device *pdev;
	struct hi3660_thermal_sensor sensors[HISI_MAX_SENSORS];
	void __iomem *thermal_base;
};

unsigned int sensor_reg_offset[HW_MAX_SENSORS] = { 0x1c, 0x5c, 0x9c, 0xdc };


static int hi3660_thermal_get_temp(void *_sensor, int *temp)
{
	struct hi3660_thermal_sensor *sensor = _sensor;
	struct hi3660_thermal_data *data = sensor->thermal;
	unsigned int idx;
	int val, average = 0, max = 0;

	if (sensor->id < HW_MAX_SENSORS) {
		val = readl(data->thermal_base + sensor_reg_offset[sensor->id]);
		val = clamp_val(val, ADC_MIN, ADC_MAX);
	} else {
		for (idx = 0; idx < HW_MAX_SENSORS; idx++) {
			val = readl(data->thermal_base
					+ sensor_reg_offset[idx]);
			val = clamp_val(val, ADC_MIN, ADC_MAX);
			average += val;
			if (val > max)
				max = val;
		}

		if (sensor->id == SENSOR_MAX)
			val = max;
		else if (sensor->id == SENSOR_AVG)
			val = average / HW_MAX_SENSORS;
	}

	*temp = ((val - ADC_MIN) * 165000) / (ADC_MAX - ADC_MIN) - 40000;

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
	int ret = 0;

	sensor->id = index;
	sensor->thermal = data;

	sensor->tzd = devm_thermal_zone_of_sensor_register(&pdev->dev,
				sensor->id, sensor, &hi3660_of_thermal_ops);
	if (IS_ERR(sensor->tzd)) {
		ret = PTR_ERR(sensor->tzd);
		sensor->tzd = NULL;
	}

	return ret;
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
	struct device *dev = &pdev->dev;
	struct hi3660_thermal_data *data;
	struct resource *res;
	int ret = 0;
	int i;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->pdev = pdev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->thermal_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->thermal_base)) {
		dev_err(dev, "failed get reg base\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, data);

	for (i = 0; i < HISI_MAX_SENSORS; ++i) {
		ret = hi3660_thermal_register_sensor(pdev, data,
						     &data->sensors[i], i);
		if (ret)
			dev_err(&pdev->dev,
				"failed to register thermal sensor%d: %d\n",
				i, ret);
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
	}

	return 0;
}

static const struct of_device_id hi3660_thermal_id_table[] = {
	{ .compatible = "hisilicon,hi3660-thermal" },
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

MODULE_AUTHOR("Tao Wang <kevin.wangtao@hisilicon.com>");
MODULE_AUTHOR("Leo Yan <leo.yan@linaro.org>");
MODULE_DESCRIPTION("hi3660 thermal driver");
MODULE_LICENSE("GPL v2");

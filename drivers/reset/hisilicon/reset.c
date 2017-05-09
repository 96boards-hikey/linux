/*
 * Copyright (c) 2016-2017 Linaro Ltd.
 * Copyright (c) 2016-2017 HiSilicon Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/mfd/syscon.h>

#include "reset.h"

struct hisi_reset_controller {
	struct reset_controller_dev rst;
	const struct hisi_reset_channel_data *channels;
	struct regmap *map;
};

#define to_hisi_reset_controller(_rst) \
	container_of(_rst, struct hisi_reset_controller, rst)

static int hisi_reset_program_hw(struct reset_controller_dev *rcdev,
				 unsigned long idx, bool assert)
{
	struct hisi_reset_controller *rc = to_hisi_reset_controller(rcdev);
	const struct hisi_reset_channel_data *ch;

	if (idx >= rcdev->nr_resets)
		return -EINVAL;

	ch = &rc->channels[idx];

	if (assert)
		return regmap_write(rc->map, ch->enable.reg,
				    GENMASK(ch->enable.msb, ch->enable.lsb));
	else
		return regmap_write(rc->map, ch->disable.reg,
				    GENMASK(ch->disable.msb, ch->disable.lsb));
}

static int hisi_reset_assert(struct reset_controller_dev *rcdev,
			     unsigned long idx)
{
	return hisi_reset_program_hw(rcdev, idx, true);
}

static int hisi_reset_deassert(struct reset_controller_dev *rcdev,
			       unsigned long idx)
{
	return hisi_reset_program_hw(rcdev, idx, false);
}

static int hisi_reset_dev(struct reset_controller_dev *rcdev,
			  unsigned long idx)
{
	int err;

	err = hisi_reset_assert(rcdev, idx);
	if (err)
		return err;

	return hisi_reset_deassert(rcdev, idx);
}

static struct reset_control_ops hisi_reset_ops = {
	.reset    = hisi_reset_dev,
	.assert   = hisi_reset_assert,
	.deassert = hisi_reset_deassert,
};

int hisi_reset_probe(struct platform_device *pdev)
{
	struct hisi_reset_controller *rc;
	struct device_node *np = pdev->dev.of_node;
	struct hisi_reset_controller_data *d;
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;

	match = of_match_device(dev->driver->of_match_table, dev);
	if (!match || !match->data)
		return -EINVAL;

	d = (struct hisi_reset_controller_data *)match->data;
	rc = devm_kzalloc(dev, sizeof(*rc), GFP_KERNEL);
	if (!rc)
		return -ENOMEM;

	rc->map = syscon_regmap_lookup_by_phandle(np, "hisi,rst-syscon");
	if (IS_ERR(rc->map)) {
		dev_err(dev, "failed to get hisi,rst-syscon\n");
		return PTR_ERR(rc->map);
	}

	rc->rst.ops = &hisi_reset_ops,
	rc->rst.of_node = np;
	rc->rst.nr_resets = d->nr_channels;
	rc->channels = d->channels;

	return reset_controller_register(&rc->rst);
}
EXPORT_SYMBOL_GPL(hisi_reset_probe);

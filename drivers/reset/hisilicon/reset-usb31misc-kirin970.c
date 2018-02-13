/*
 * Copyright (c) 2017-2018 HiSilicon Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

struct usb31misc_reset_controller {
	struct reset_controller_dev rst;
	struct regmap *map;
};

#define to_usb31misc_reset_controller(_rst) \
	container_of(_rst, struct usb31misc_reset_controller, rst)

static int usb31misc_reset_program_hw(struct reset_controller_dev *rcdev,
				   unsigned long idx, bool assert)
{
	struct usb31misc_reset_controller *rc =
		to_usb31misc_reset_controller(rcdev);
	unsigned int offset = idx >> 8;
	unsigned int mask = BIT(idx & 0x1f);

	if (assert)
		return regmap_update_bits(rc->map, offset, mask, 0);
	else
		return regmap_update_bits(rc->map, offset, mask, mask);
}

static int usb31misc_reset_assert(struct reset_controller_dev *rcdev,
			       unsigned long idx)
{
	return usb31misc_reset_program_hw(rcdev, idx, true);
}

static int usb31misc_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long idx)
{
	return usb31misc_reset_program_hw(rcdev, idx, false);
}

static int usb31misc_reset_dev(struct reset_controller_dev *rcdev,
			    unsigned long idx)
{
	int err;

	err = usb31misc_reset_assert(rcdev, idx);
	if (err)
		return err;

	return usb31misc_reset_deassert(rcdev, idx);
}

static struct reset_control_ops usb31misc_reset_ops = {
	.reset    = usb31misc_reset_dev,
	.assert   = usb31misc_reset_assert,
	.deassert = usb31misc_reset_deassert,
};

static int usb31misc_reset_xlate(struct reset_controller_dev *rcdev,
			      const struct of_phandle_args *reset_spec)
{
	unsigned int offset, bit;

	offset = reset_spec->args[0];
	bit = reset_spec->args[1];

	return (offset << 8) | bit;
}

static int usb31misc_reset_probe(struct platform_device *pdev)
{
	struct usb31misc_reset_controller *rc;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;

	rc = devm_kzalloc(dev, sizeof(*rc), GFP_KERNEL);
	if (!rc)
		return -ENOMEM;

	rc->map = syscon_regmap_lookup_by_phandle(np, "hisi,rst-syscon");
	if (IS_ERR(rc->map)) {
		dev_err(dev, "failed to get usb31misc,rst-syscon\n");
		return PTR_ERR(rc->map);
	}

	rc->rst.ops = &usb31misc_reset_ops,
	rc->rst.of_node = np;
	rc->rst.of_reset_n_cells = 2;
	rc->rst.of_xlate = usb31misc_reset_xlate;

	return reset_controller_register(&rc->rst);
}

static const struct of_device_id usb31misc_reset_match[] = {
	{ .compatible = "hisilicon,kirin970-usb31-misc-reset", },
	{},
};
MODULE_DEVICE_TABLE(of, usb31misc_reset_match);

static struct platform_driver usb31misc_reset_driver = {
	.probe = usb31misc_reset_probe,
	.driver = {
		.name = "usb31misc-reset",
		.of_match_table = usb31misc_reset_match,
	},
};

static int __init usb31misc_reset_init(void)
{
	return platform_driver_register(&usb31misc_reset_driver);
}
arch_initcall(usb31misc_reset_init);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:kirin970-usb31misc-reset");
MODULE_DESCRIPTION("HiSilicon Kirin970 USB3 Misc Reset Driver");

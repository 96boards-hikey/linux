/*
 * Copyright (c) 2016-2017 Linaro Ltd.
 * Copyright (c) 2016-2017 HiSilicon Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <dt-bindings/reset/hisi,hi3660-resets.h>

#include "reset.h"

static const struct hisi_reset_channel_data hi3660_iomcu_rst[] = {
	[HI3660_RST_I2C0] = HISI_RST_SEP(0x20, 3),
	[HI3660_RST_I2C1] = HISI_RST_SEP(0x20, 4),
	[HI3660_RST_I2C2] = HISI_RST_SEP(0x20, 5),
	[HI3660_RST_I2C6] = HISI_RST_SEP(0x20, 27),
};

static struct hisi_reset_controller_data hi3660_iomcu_controller = {
	.nr_channels = ARRAY_SIZE(hi3660_iomcu_rst),
	.channels = hi3660_iomcu_rst,
};

static const struct hisi_reset_channel_data hi3660_crgctrl_rst[] = {
	[HI3660_RST_I2C3] = HISI_RST_SEP(0x78, 7),
	[HI3660_RST_I2C4] = HISI_RST_SEP(0x78, 27),
	[HI3660_RST_I2C7] = HISI_RST_SEP(0x60, 14),
	[HI3660_RST_SD] = HISI_RST_SEP(0x90, 18),
	[HI3660_RST_SDIO] = HISI_RST_SEP(0x90, 20),
	[HI3660_RST_UFS] = HISI_RST_SEP(0x84, 12),
	[HI3660_RST_UFS_ASSERT] = HISI_RST_SEP(0x84, 7),
	[HI3660_RST_PCIE_SYS] = HISI_RST_SEP(0x84, 26),
	[HI3660_RST_PCIE_PHY] = HISI_RST_SEP(0x84, 27),
	[HI3660_RST_PCIE_BUS] = HISI_RST_SEP(0x84, 31),
	[HI3660_RST_USB3OTG_PHY] = HISI_RST_SEP(0x90, 3),
	[HI3660_RST_USB3OTG] = HISI_RST_SEP(0x90, 5),
	[HI3660_RST_USB3OTG_32K] = HISI_RST_SEP(0x90, 6),
	[HI3660_RST_USB3OTG_AHB] = HISI_RST_SEP(0x90, 7),
	[HI3660_RST_USB3OTG_MUX] = HISI_RST_SEP(0x90, 8),
};

static struct hisi_reset_controller_data hi3660_crgctrl_controller = {
	.nr_channels = ARRAY_SIZE(hi3660_crgctrl_rst),
	.channels = hi3660_crgctrl_rst,
};

static const struct of_device_id hi3660_reset_match[] = {
	{ .compatible = "hisilicon,hi3660-reset-crgctrl",
	  .data = &hi3660_crgctrl_controller, },
	{ .compatible = "hisilicon,hi3660-reset-iomcu",
	  .data = &hi3660_iomcu_controller, },
	{},
};
MODULE_DEVICE_TABLE(of, hi3660_reset_match);

static struct platform_driver hi3660_reset_driver = {
	.probe = hisi_reset_probe,
	.driver = {
		.name = "reset-hi3660",
		.of_match_table = hi3660_reset_match,
	},
};

static int __init hi3660_reset_init(void)
{
	return platform_driver_register(&hi3660_reset_driver);
}
arch_initcall(hi3660_reset_init);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:hi3660-reset");
MODULE_DESCRIPTION("HiSilicon Hi3660 Reset Driver");

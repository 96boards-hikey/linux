/*
 * Copyright (c) 2016-2017 Linaro Ltd.
 * Copyright (c) 2016-2017 HiSilicon Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HISILICON_RESET_H
#define __HISILICON_RESET_H

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

/* reset separated register offset is 0x4 */
#define HISI_RST_SEP(off, bit)					\
	{ .enable	= REG_FIELD(off, bit, bit),		\
	  .disable	= REG_FIELD(off + 0x4, bit, bit),	\
	  .status	= REG_FIELD(off + 0x8, bit, bit), }

struct hisi_reset_channel_data {
	struct reg_field enable;
	struct reg_field disable;
	struct reg_field status;
};

struct hisi_reset_controller_data {
	int nr_channels;
	const struct hisi_reset_channel_data *channels;
};

int hisi_reset_probe(struct platform_device *pdev);

#endif /* __HISILICON_RESET_H */

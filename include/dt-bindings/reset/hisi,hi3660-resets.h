/*
 * Copyright (c) 2016-2017 Linaro Ltd.
 * Copyright (c) 2016-2017 HiSilicon Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _DT_BINDINGS_RESET_CONTROLLER_HI3660
#define _DT_BINDINGS_RESET_CONTROLLER_HI3660

/* reset in iomcu */
#define HI3660_RST_I2C0		0
#define HI3660_RST_I2C1		1
#define HI3660_RST_I2C2		2
#define HI3660_RST_I2C6		3


/* reset in crgctrl */
#define HI3660_RST_I2C3		0
#define HI3660_RST_I2C4		1
#define HI3660_RST_I2C7		2
#define HI3660_RST_SD		3
#define HI3660_RST_SDIO		4
#define HI3660_RST_UFS		5
#define HI3660_RST_UFS_ASSERT	6
#define HI3660_RST_PCIE_SYS	7
#define HI3660_RST_PCIE_PHY	8
#define HI3660_RST_PCIE_BUS	9
#define HI3660_RST_USB3OTG_PHY  10
#define HI3660_RST_USB3OTG	11
#define HI3660_RST_USB3OTG_32K	12
#define HI3660_RST_USB3OTG_AHB	13
#define HI3660_RST_USB3OTG_MUX	14

#endif /*_DT_BINDINGS_RESET_CONTROLLER_HI3660*/

/*
 * PCIe host controller driver for Kirin 960 SoCs
 *
 * Copyright (C) 2015 Huawei Electronics Co., Ltd.
 *		http://www.huawei.com
 *
 * Author: Xiaowei Song <songxiaowei@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PCIE_KIRIN_H
#define _PCIE_KIRIN_H

#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <asm/compiler.h>
#include <linux/compiler.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/of_pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/types.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/pci_regs.h>

#include "pcie-designware.h"

#define to_kirin_pcie(x)	container_of(x, struct kirin_pcie, pp)

#define REF_CLK_FREQ 100000000

/* PCIe ELBI registers */
#define SOC_PCIECTRL_CTRL0_ADDR 0x000
#define SOC_PCIECTRL_CTRL1_ADDR 0x004
#define SOC_PCIEPHY_CTRL2_ADDR 0x008
#define SOC_PCIEPHY_CTRL3_ADDR 0x00c
#define PCIE_ELBI_SLV_DBI_ENABLE	(0x1 << 21)

#define PCIE_APP_LTSSM_ENABLE		0x01c
#define PCIE_ELBI_RDLH_LINKUP		0x400
#define PCIE_LINKUP_ENABLE		(0x8020)
#define PCIE_LTSSM_ENABLE_BIT	  (0x1 << 11)

struct kirin_pcie {
	void __iomem		*apb_base;
	void __iomem		*phy_base;
	struct regmap *crgctrl;
	struct regmap *sysctrl;
	struct clk			*apb_sys_clk;
	struct clk			*apb_phy_clk;
	struct clk			*phy_ref_clk;
	struct clk			*pcie_aclk;
	struct clk			*pcie_aux_clk;
	int                 gpio_id_reset;
	struct  pcie_port	pp;
};

#endif


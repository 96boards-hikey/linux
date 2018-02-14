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
#include <linux/of_device.h>
#include <linux/pci_regs.h>
#include <linux/regulator/consumer.h>

#include "pcie-designware.h"

#define to_kirin_pcie(x)	container_of(x, struct kirin_pcie, pp)

#define REF_CLK_FREQ              100000000

/* PCIe CTRL registers */
#define SOC_PCIECTRL_CTRL0_ADDR   0x000
#define SOC_PCIECTRL_CTRL1_ADDR   0x004
#define SOC_PCIECTRL_CTRL7_ADDR   0x01c
#define SOC_PCIECTRL_CTRL12_ADDR  0x030
#define SOC_PCIECTRL_CTRL20_ADDR  0x050
#define SOC_PCIECTRL_CTRL21_ADDR  0x054
#define SOC_PCIECTRL_STATE0_ADDR  0x400

/* PCIe PHY registers */
#define SOC_PCIEPHY_CTRL0_ADDR    0x000
#define SOC_PCIEPHY_CTRL1_ADDR    0x004
#define SOC_PCIEPHY_CTRL2_ADDR    0x008
#define SOC_PCIEPHY_CTRL3_ADDR    0x00c
#define SOC_PCIEPHY_CTRL38_ADDR   0x0098
#define SOC_PCIEPHY_STATE0_ADDR   0x400

#define PCIE_LINKUP_ENABLE            (0x8020)
#define PCIE_ELBI_SLV_DBI_ENABLE      (0x1 << 21)
#define PCIE_LTSSM_ENABLE_BIT         (0x1 << 11)
#define PCIEPHY_RESET_BIT             (0x1 << 17)
#define PCIEPHY_PIPE_LINE0_RESET_BIT  (0x1 << 19)

#define PORT_MSI_CTRL_ADDR            0x820
#define PORT_MSI_CTRL_UPPER_ADDR      0x824
#define PORT_MSI_CTRL_INT0_ENABLE     0x828

struct kirin_pcie {
	void __iomem   *apb_base;
	void __iomem   *phy_base;
	struct regmap  *crgctrl;
	struct regmap  *sysctrl;
	struct regmap  *pmctrl;
	struct clk     *apb_sys_clk;
	struct clk     *apb_phy_clk;
	struct clk     *phy_ref_clk;
	struct clk     *pcie_aclk;
	struct clk     *pcie_aux_clk;
	int            gpio_id_reset[4];
	int            gpio_id_clkreq[3];
	u32            eye_param[5];
	u32            aer_config;
	u32            msi_controller_config[3];
	struct pcie_port            pp;
	struct pci_dev              *rc_dev;
	struct pci_saved_state      *rc_saved_state;
	const struct kirin_pcie_ops *pcie_ops;
	struct regulator            *ldo33;
};

struct kirin_pcie_ops {
	int (*pcie_probe)(struct kirin_pcie *pcie);
	int (*pcie_suspend_noirq)(struct device *dev);
	int (*pcie_resume_noirq)(struct device *dev);
	void (*kirin_phy_writel)(struct kirin_pcie *pcie, u32 val, u32 reg);
	u32 (*kirin_phy_readl)(struct kirin_pcie *pcie, u32 reg);
};

static inline void kirin_elb_writel(struct kirin_pcie *pcie, u32 val, u32 reg)
{
	writel(val, pcie->apb_base + reg);
}

static inline u32 kirin_elb_readl(struct kirin_pcie *pcie, u32 reg)
{
	return readl(pcie->apb_base + reg);
}

static inline void kirin_phy_writel(struct kirin_pcie *pcie, u32 val, u32 reg)
{
	pcie->pcie_ops->kirin_phy_writel(pcie, val, reg);
}

static inline u32 kirin_phy_readl(struct kirin_pcie *pcie, u32 reg)
{
	return pcie->pcie_ops->kirin_phy_readl(pcie, reg);
}

static inline void kirin_natural_phy_writel(struct kirin_pcie * pcie, u32 val, u32 reg)
{
	writel(val, pcie->phy_base + reg * 4);
}

static inline u32 kirin_natural_phy_readl(struct kirin_pcie *pcie, u32 reg)
{
	return readl(pcie->phy_base + reg * 4);
}

int kirin_pcie_save_rc_cfg(struct kirin_pcie *pcie);
int kirin_pcie_restore_rc_cfg(struct kirin_pcie *pcie);

extern const struct kirin_pcie_ops kirin960_pcie_ops;
extern const struct kirin_pcie_ops kirin970_pcie_ops;
#endif


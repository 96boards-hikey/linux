/*
 * PCIe host controller driver for Kirin960 Phone SoCs
 *
 * Copyright (C) 2015 Hilisicon Electronics Co., Ltd.
 *		http://www.huawei.com
 *
 * Author: Xiaowei Song <songxiaowei@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pcie-kirin.h"

static inline void kirin960_phy_writel(struct kirin_pcie *pcie, u32 val, u32 reg)
{
	writel(val, pcie->phy_base + 0x20000 + reg);
}

static inline u32 kirin960_phy_readl(struct kirin_pcie *pcie, u32 reg)
{
	return readl(pcie->phy_base + 0x20000 + reg);
}

static int32_t kirin960_pcie_get_clk(struct kirin_pcie *pcie,
				  struct platform_device *pdev)
{
	pcie->phy_ref_clk = devm_clk_get(&pdev->dev, "pcie_phy_ref");
	if (IS_ERR(pcie->phy_ref_clk))
		return PTR_ERR(pcie->phy_ref_clk);

	pcie->pcie_aux_clk = devm_clk_get(&pdev->dev, "pcie_aux");
	if (IS_ERR(pcie->pcie_aux_clk))
		return PTR_ERR(pcie->pcie_aux_clk);

	pcie->apb_phy_clk = devm_clk_get(&pdev->dev, "pcie_apb_phy");
	if (IS_ERR(pcie->apb_phy_clk))
		return PTR_ERR(pcie->apb_phy_clk);

	pcie->apb_sys_clk = devm_clk_get(&pdev->dev, "pcie_apb_sys");
	if (IS_ERR(pcie->apb_sys_clk))
		return PTR_ERR(pcie->apb_sys_clk);

	pcie->pcie_aclk = devm_clk_get(&pdev->dev, "pcie_aclk");
	if (IS_ERR(pcie->pcie_aclk))
		return PTR_ERR(pcie->pcie_aclk);

	return 0;
}

static int32_t kirin960_pcie_get_resource(struct pcie_port *pp,
				       struct platform_device *pdev)
{
	struct resource *apb;
	struct resource *phy;
	struct resource *dbi;
	struct kirin_pcie *pcie = to_kirin_pcie(pp);

	apb = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apb");
	pcie->apb_base = devm_ioremap_resource(&pdev->dev, apb);
	if (IS_ERR(pcie->apb_base))
		return PTR_ERR(pcie->apb_base);

	phy = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	pcie->phy_base = devm_ioremap_resource(&pdev->dev, phy);
	if (IS_ERR(pcie->phy_base))
		return PTR_ERR(pcie->phy_base);

	dbi = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pp->dbi_base = devm_ioremap_resource(&pdev->dev, dbi);
	if (IS_ERR(pp->dbi_base))
		return PTR_ERR(pp->dbi_base);

	pcie->crgctrl =
		syscon_regmap_lookup_by_compatible("hisilicon,hi3660-crgctrl");
	if (IS_ERR(pcie->crgctrl))
		return PTR_ERR(pcie->crgctrl);

	pcie->sysctrl =
		syscon_regmap_lookup_by_compatible("hisilicon,hi3660-sctrl");
	if (IS_ERR(pcie->sysctrl))
		return PTR_ERR(pcie->sysctrl);

	return 0;
}

static int kirin960_pcie_phy_init(struct kirin_pcie *pcie)
{
	u32 reg_val;
	u32 pipe_clk_stable = 0x1 << 19;
	u32 time = 10;

	reg_val = kirin_phy_readl(pcie, 0x4);
	reg_val &= ~(0x1 << 8);
	kirin_phy_writel(pcie, reg_val, 0x4);

	reg_val = kirin_phy_readl(pcie, 0x0);
	reg_val &= ~(0x1 << 22);
	kirin_phy_writel(pcie, reg_val, 0x0);
	udelay(10);

	reg_val = kirin_phy_readl(pcie, 0x4);
	reg_val &= ~(0x1 << 16);
	kirin_phy_writel(pcie, reg_val, 0x4);

	reg_val = kirin_phy_readl(pcie, 0x400);
	while (reg_val & pipe_clk_stable) {
		udelay(100);
		if (time == 0) {
			dev_err(pcie->pp.dev, "PIPE clk is not stable\n");
			return -EINVAL;
		}
		time--;
		reg_val = kirin_phy_readl(pcie, 0x400);
	}

	return 0;
}

static void kirin960_pcie_oe_enable(struct kirin_pcie *pcie)
{
	u32 val;

	regmap_read(pcie->sysctrl, 0x1a4, &val);
	val |= 0xF0F400;
	val &= ~(0x3 << 28);
	regmap_write(pcie->sysctrl, 0x1a4, val);
}

static int kirin960_pcie_clk_ctrl(struct kirin_pcie *pcie, bool enable)
{
	int ret = 0;

	if (!enable)
		goto close_clk;

	ret = clk_set_rate(pcie->phy_ref_clk, REF_CLK_FREQ);
	if (ret)
		return ret;

	ret = clk_prepare_enable(pcie->phy_ref_clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(pcie->apb_sys_clk);
	if (ret)
		goto apb_sys_fail;

	ret = clk_prepare_enable(pcie->apb_phy_clk);
	if (ret)
		goto apb_phy_fail;

	ret = clk_prepare_enable(pcie->pcie_aclk);
	if (ret)
		goto aclk_fail;

	ret = clk_prepare_enable(pcie->pcie_aux_clk);
	if (ret)
		goto aux_clk_fail;

	return 0;
close_clk:
	clk_disable_unprepare(pcie->pcie_aux_clk);
aux_clk_fail:
	clk_disable_unprepare(pcie->pcie_aclk);
aclk_fail:
	clk_disable_unprepare(pcie->apb_phy_clk);
apb_phy_fail:
	clk_disable_unprepare(pcie->apb_sys_clk);
apb_sys_fail:
	clk_disable_unprepare(pcie->phy_ref_clk);
	return ret;
}

static int kirin960_pcie_power_on(struct kirin_pcie *pcie)
{
	int ret;

	/* Power supply for Host */
	regmap_write(pcie->sysctrl, 0x60, 0x10);
	udelay(100);
	kirin960_pcie_oe_enable(pcie);

	ret = kirin960_pcie_clk_ctrl(pcie, true);
	if (ret)
		return ret;

	/* deasset PCIeCtrl&PCIePHY */
	regmap_write(pcie->sysctrl, 0x44, 0x30);
	regmap_write(pcie->crgctrl, 0x88, 0x8c000000);
	regmap_write(pcie->sysctrl, 0x190, 0x184000);

	ret = kirin960_pcie_phy_init(pcie);
	if (ret)
		goto close_clk;

	/* perst assert */
	mdelay(20);
	if (!gpio_request(pcie->gpio_id_reset[0], "pcie_perst")) {
		ret = gpio_direction_output(pcie->gpio_id_reset[0], 1);
		if (ret)
			goto close_clk;
		mdelay(10);

		return 0;
	}

close_clk:
	kirin960_pcie_clk_ctrl(pcie, false);
	return -1;
}

static int kirin960_pcie_probe(struct kirin_pcie *pcie)
{
	struct platform_device *pdev;
	struct pcie_port *pp;
	int ret;

	pp = &pcie->pp;
	pdev = to_platform_device(pp->dev);

	ret = kirin960_pcie_get_clk(pcie, pdev);
	if (ret != 0)
		return -ENODEV;

	ret = kirin960_pcie_get_resource(pp, pdev);
	if (ret != 0)
		return -ENODEV;

	pcie->gpio_id_reset[0] = of_get_named_gpio(pdev->dev.of_node,
			"reset-gpios", 0);
	if (pcie->gpio_id_reset[0] < 0)
		return -ENODEV;

	ret = kirin960_pcie_power_on(pcie);
	if (ret)
		return ret;

	return 0;
}

const struct kirin_pcie_ops kirin960_pcie_ops = {
	.pcie_probe = kirin960_pcie_probe,
	.kirin_phy_writel = kirin960_phy_writel,
	.kirin_phy_readl = kirin960_phy_readl,
};

 EXPORT_SYMBOL_GPL(kirin960_pcie_ops);
/*
 * PCIe host controller driver for Kirin Phone SoCs
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

struct kirin_pcie *g_kirin_pcie;

static int kirin_pcie_link_up(struct pcie_port *pp);

static inline void kirin_elb_writel(struct kirin_pcie *pcie, u32 val, u32 reg)
{
	writel(val, pcie->apb_base + reg);
}

static inline u32 kirin_elb_readl(struct kirin_pcie *pcie, u32 reg)
{
	return readl(pcie->apb_base + reg);
}

/*Registers in PCIePHY*/
static inline void kirin_phy_writel(struct kirin_pcie *pcie, u32 val, u32 reg)
{
	writel(val, pcie->phy_base + reg);
}

static inline u32 kirin_phy_readl(struct kirin_pcie *pcie, u32 reg)
{
	return readl(pcie->phy_base + reg);
}

static int32_t kirin_pcie_get_clk(struct kirin_pcie *pcie,
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

static int32_t kirin_pcie_get_resource(struct pcie_port *pp,
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


	pcie->crgctrl = syscon_regmap_lookup_by_compatible("hisilicon,hi3660-crgctrl");
	if (IS_ERR(pcie->crgctrl))
		return PTR_ERR(pcie->crgctrl);


	pcie->sysctrl = syscon_regmap_lookup_by_compatible("hisilicon,hi3660-sctrl");
	if (IS_ERR(pcie->sysctrl))
		return PTR_ERR(pcie->sysctrl);

	return 0;
}

static int32_t kirin_pcie_get_eyeparam(struct kirin_pcie *pcie,
				       struct platform_device *pdev)
{
	int ret;
	u32 eye_param;
	struct device_node *np = pdev->dev.of_node;

	ret = of_property_read_u32(np, "eye_param_ctrl2", &eye_param);
	if (ret)
		pcie->eye_param_ctrl2 = 0x0;

	ret = of_property_read_u32(np, "eye_param_ctrl3", &eye_param);
	if (ret)
		pcie->eye_param_ctrl3 = 0x0;

	return 0;
}

static void set_eye_param(struct kirin_pcie *pcie)
{
	if (pcie->eye_param_ctrl2)
		kirin_phy_writel(pcie, pcie->eye_param_ctrl2, 0x8);

	if (pcie->eye_param_ctrl3)
		kirin_phy_writel(pcie, pcie->eye_param_ctrl3, 0xc);
}

static int kirin_pcie_phy_init(struct kirin_pcie *pcie)
{
	u32 reg_val;
	u32 pipe_clk_stable = 0x1 << 19;
	u32 time = 10;

	set_eye_param(pcie);

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

static void kirin_pcie_oe_enable(struct kirin_pcie *pcie)
{
	u32 val;

	regmap_read(pcie->sysctrl, 0x1a4, &val);
	val |= 0xF0F400;
	val &= ~(0x3 << 28);
	regmap_write(pcie->sysctrl, 0x1a4, val);
}

static int kirin_pcie_clk_enable(struct kirin_pcie *pcie)
{
	int ret;

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

int kirin_pcie_power_on(struct kirin_pcie *pcie)
{
	int ret;

	/*Power supply for Host*/
	regmap_write(pcie->sysctrl, 0x60, 0x10);
	udelay(100);
	kirin_pcie_oe_enable(pcie);

	ret = kirin_pcie_clk_enable(pcie);
	if (ret)
		return ret;

	/*deasset PCIeCtrl&PCIePHY*/
	regmap_write(pcie->sysctrl, 0x44, 0x30);
	regmap_write(pcie->crgctrl, 0x88, 0x8c000000);
	regmap_write(pcie->sysctrl, 0x190, 0x184000);

	ret = kirin_pcie_phy_init(pcie);
	if (ret)
		return ret;

	/*perst assert*/
	mdelay(20);
	ret = gpio_request(pcie->gpio_id_reset, "pcie_perst");
	if (ret)
		return ret;

	ret = gpio_direction_output(pcie->gpio_id_reset, 1);
	if (ret)
		return ret;
	mdelay(10);

	return 0;
}

static void kirin_pcie_sideband_dbi_w_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct kirin_pcie *pcie = to_kirin_pcie(pp);

	val = kirin_elb_readl(pcie, SOC_PCIECTRL_CTRL0_ADDR);
	if (on)
		val = val | PCIE_ELBI_SLV_DBI_ENABLE;
	else
		val = val & ~PCIE_ELBI_SLV_DBI_ENABLE;

	kirin_elb_writel(pcie, val, SOC_PCIECTRL_CTRL0_ADDR);
}

static void kirin_pcie_sideband_dbi_r_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct kirin_pcie *pcie = to_kirin_pcie(pp);

	val = kirin_elb_readl(pcie, SOC_PCIECTRL_CTRL1_ADDR);
	if (on)
		val = val | PCIE_ELBI_SLV_DBI_ENABLE;
	else
		val = val & ~PCIE_ELBI_SLV_DBI_ENABLE;

	kirin_elb_writel(pcie, val, SOC_PCIECTRL_CTRL1_ADDR);
}

static int kirin_pcie_establish_link(struct pcie_port *pp)
{
	int count = 0;
	struct kirin_pcie *pcie = to_kirin_pcie(pp);

	if (kirin_pcie_link_up(pp))
		return 0;

	dw_pcie_setup_rc(pp);

	/* assert LTSSM enable */
	kirin_elb_writel(pcie, PCIE_LTSSM_ENABLE_BIT,
			  PCIE_APP_LTSSM_ENABLE);

	/* check if the link is up or not */
	while (!kirin_pcie_link_up(pp)) {
		mdelay(1);
		count++;
		if (count == 20) {
			dev_err(pp->dev, "Link Fail\n");
			return -EINVAL;
		}
	}

	return 0;
}

static irqreturn_t kirin_pcie_msi_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

	return dw_handle_msi_irq(pp);
}

static void kirin_pcie_msi_init(struct pcie_port *pp)
{
	dw_pcie_msi_init(pp);
}

static void kirin_pcie_enable_interrupts(struct pcie_port *pp)
{
	if (IS_ENABLED(CONFIG_PCI_MSI))
		kirin_pcie_msi_init(pp);
}

static int kirin_pcie_rd_own_conf(struct pcie_port *pp,
				  int where, int size, u32 *val)
{
	kirin_pcie_sideband_dbi_r_mode(pp, true);

	*val = readl(pp->dbi_base + (where & ~0x3));
	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;
	else if (size != 4)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	kirin_pcie_sideband_dbi_r_mode(pp, false);

	return PCIBIOS_SUCCESSFUL;
}

static int kirin_pcie_wr_own_conf(struct pcie_port *pp,
				  int where, int size, u32 val)
{
	kirin_pcie_sideband_dbi_w_mode(pp, true);

	if (size == 4)
		writel(val, pp->dbi_base + (where & ~0x3));
	else if (size == 2)
		writew(val, pp->dbi_base + (where & ~0x3) + (where & 2));
	else if (size == 1)
		writeb(val, pp->dbi_base + (where & ~0x3) + (where & 3));
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	kirin_pcie_sideband_dbi_w_mode(pp, false);

	return PCIBIOS_SUCCESSFUL;
}

static void kirin_pcie_readl_rc(struct pcie_port *pp,
				void __iomem *dbi_base, u32 *val)
{
	kirin_pcie_sideband_dbi_r_mode(pp, true);
	*val = readl(dbi_base);
	kirin_pcie_sideband_dbi_r_mode(pp, false);
}

static void kirin_pcie_writel_rc(struct pcie_port *pp,
				 u32 val, void __iomem *dbi_base)
{
	kirin_pcie_sideband_dbi_w_mode(pp, true);
	writel(val, dbi_base);
	kirin_pcie_sideband_dbi_w_mode(pp, false);
}

static int kirin_pcie_link_up(struct pcie_port *pp)
{
	struct kirin_pcie *pcie = to_kirin_pcie(pp);
	u32 val = kirin_elb_readl(pcie, PCIE_ELBI_RDLH_LINKUP);

	if ((val & PCIE_LINKUP_ENABLE) == PCIE_LINKUP_ENABLE)
		return 1;

	return 0;
}

static void kirin_pcie_host_init(struct pcie_port *pp)
{
	if (kirin_pcie_establish_link(pp))
		return;

	kirin_pcie_enable_interrupts(pp);

	return;
}

static struct pcie_host_ops kirin_pcie_host_ops = {
	.readl_rc = kirin_pcie_readl_rc,
	.writel_rc = kirin_pcie_writel_rc,
	.rd_own_conf = kirin_pcie_rd_own_conf,
	.wr_own_conf = kirin_pcie_wr_own_conf,
	.link_up = kirin_pcie_link_up,
	.host_init = kirin_pcie_host_init,
};

static int __init kirin_add_pcie_port(struct pcie_port *pp,
					   struct platform_device *pdev)
{
	int ret;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq(pdev, 0);
		if (!pp->msi_irq) {
			dev_err(&pdev->dev, "failed to get msi irq\n");
			return -ENODEV;
		}
		ret = devm_request_irq(&pdev->dev, pp->msi_irq,
					kirin_pcie_msi_irq_handler,
					IRQF_SHARED | IRQF_TRIGGER_RISING,
					"kirin_pcie", pp);
		if (ret) {
			dev_err(&pdev->dev, "failed to request msi irq\n");
			return ret;
		}
	}

	pp->root_bus_nr = -1;
	pp->ops = &kirin_pcie_host_ops;

	return 0;
}

static int kirin_pcie_probe(struct platform_device *pdev)
{
	struct kirin_pcie *pcie;
	struct pcie_port *pp;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "NULL node\n");
		return -EINVAL;
	}

	pcie = devm_kzalloc(&pdev->dev,
			    sizeof(struct kirin_pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pp = &pcie->pp;
	g_kirin_pcie = pcie;

	pp->dev = &(pdev->dev);

	ret = kirin_pcie_get_eyeparam(pcie, pdev);
	if (ret)
		return ret;

	ret = kirin_pcie_get_clk(pcie, pdev);
	if (ret)
		return ret;

	ret = kirin_pcie_get_resource(pp, pdev);
	if (ret)
		return ret;

	pcie->gpio_id_reset = of_get_named_gpio(pdev->dev.of_node,
			"reset-gpio", 0);
	if (pcie->gpio_id_reset < 0)
		return -ENODEV;

	ret = kirin_add_pcie_port(pp, pdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pcie);

	dev_dbg(&pdev->dev, "probe Done\n");
	return 0;

}

/*
* Enumerate Function: called by EP
* param: void
*/
int kirin_pcie_enumerate(void)
{
	int ret;
	struct pcie_port *pp;
	struct kirin_pcie *pcie;

	pcie = g_kirin_pcie;
	pp = &pcie->pp;

	/*Power on RC*/
	ret = kirin_pcie_power_on(pcie);
	if (ret)
		return ret;

	ret = dw_pcie_host_init(pp);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(kirin_pcie_enumerate);

static const struct of_device_id kirin_pcie_match[] = {
	{ .compatible = "hisilicon,kirin-pcie" },
	{},
};
MODULE_DEVICE_TABLE(of, kirin_pcie_match);

struct platform_driver kirin_pcie_driver = {
	.probe			= kirin_pcie_probe,
	.driver			= {
		.name			= "Kirin-pcie",
		.owner			= THIS_MODULE,
		.of_match_table = kirin_pcie_match,
	},
};

module_platform_driver(kirin_pcie_driver);

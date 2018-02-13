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
	int ret;

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

	return ret;
}

static u32 kirin_pcie_readl_rc(struct pcie_port *pp, u32 reg)
{
	u32 val;

	kirin_pcie_sideband_dbi_r_mode(pp, true);
	val = readl(pp->dbi_base + reg);
	kirin_pcie_sideband_dbi_r_mode(pp, false);

	return val;
}

static void kirin_pcie_writel_rc(struct pcie_port *pp, u32 reg, u32 val)
{
	kirin_pcie_sideband_dbi_w_mode(pp, true);
	writel(val, pp->dbi_base + reg);
	kirin_pcie_sideband_dbi_w_mode(pp, false);
}

static int kirin_pcie_link_up(struct pcie_port *pp)
{
	struct kirin_pcie *pcie = to_kirin_pcie(pp);
	u32 val = kirin_elb_readl(pcie, SOC_PCIECTRL_STATE0_ADDR);

	if ((val & PCIE_LINKUP_ENABLE) == PCIE_LINKUP_ENABLE)
		return 1;

	return 0;
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
			 SOC_PCIECTRL_CTRL7_ADDR);

	/* check if the link is up or not */
	while (!kirin_pcie_link_up(pp)) {
		mdelay(1);
		count++;
		if (count == 1000) {
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
					"kirin_pcie_msi", pp);
		if (ret) {
			dev_err(&pdev->dev, "failed to request msi irq\n");
			return ret;
		}
	}

	pp->root_bus_nr = -1;
	pp->ops = &kirin_pcie_host_ops;

	ret = dw_pcie_host_init(pp);

	return ret;
}

int kirin_pcie_save_rc_cfg(struct kirin_pcie *pcie)
{
	int ret;
	u32 val = 0;
	int aer_pos;
	struct pcie_port *pp;

	pp = &(pcie->pp);

	kirin_pcie_rd_own_conf(pp, PORT_MSI_CTRL_ADDR, 4, &val);
	pcie->msi_controller_config[0] = val;
	kirin_pcie_rd_own_conf(pp, PORT_MSI_CTRL_UPPER_ADDR, 4, &val);
	pcie->msi_controller_config[1] = val;
	kirin_pcie_rd_own_conf(pp, PORT_MSI_CTRL_INT0_ENABLE, 4, &val);
	pcie->msi_controller_config[2] = val;

	aer_pos = pci_find_ext_capability(pcie->rc_dev, PCI_EXT_CAP_ID_ERR);
	if (!aer_pos ) {
		dev_err(pcie->pp.dev, "Failed to get RC PCI_EXT_CAP_ID_ERR\n");
		return -EINVAL;
	}

	pci_read_config_dword(pcie->rc_dev, aer_pos + PCI_ERR_ROOT_COMMAND,
		&pcie->aer_config);

	ret = pci_save_state(pcie->rc_dev);
	if (ret) {
		dev_err(pcie->pp.dev, "Failed to save state of RC\n");
		return -EINVAL;
	}
	pcie->rc_saved_state = pci_store_saved_state(pcie->rc_dev);

	return 0;
}

int kirin_pcie_restore_rc_cfg(struct kirin_pcie *pcie)
{
	struct pcie_port *pp;
	int aer_pos;

	pp = &(pcie->pp);

	kirin_pcie_wr_own_conf(pp, PORT_MSI_CTRL_ADDR,
			4, pcie->msi_controller_config[0]);
	kirin_pcie_wr_own_conf(pp, PORT_MSI_CTRL_UPPER_ADDR,
			4, pcie->msi_controller_config[1]);
	kirin_pcie_wr_own_conf(pp, PORT_MSI_CTRL_INT0_ENABLE,
			4, pcie->msi_controller_config[2]);

	aer_pos = pci_find_ext_capability(pcie->rc_dev, PCI_EXT_CAP_ID_ERR);
	if (!aer_pos ) {
		dev_err(pcie->pp.dev, "Failed to get RC PCI_EXT_CAP_ID_ERR\n");
		return -EINVAL;
	}

	pci_write_config_dword(pcie->rc_dev, aer_pos + PCI_ERR_ROOT_COMMAND,
		pcie->aer_config);

	pci_load_saved_state(pcie->rc_dev, pcie->rc_saved_state);
	pci_restore_state(pcie->rc_dev);

	return 0;
}

static int kirin_pcie_probe(struct platform_device *pdev)
{
	struct kirin_pcie *pcie;
	struct pcie_port *pp;
	int ret;
	u32 dev_id;
	u32 vendor_id;
	int (*pcie_probe)(struct kirin_pcie *);

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
	pp->dev = &pdev->dev;

	pcie->pcie_ops= of_device_get_match_data(pp->dev);
	if (!pcie->pcie_ops) {
		dev_err(&pdev->dev, "failed to get pcie_ops\n");
		return -EINVAL;
	}

	pcie_probe = pcie->pcie_ops->pcie_probe;
	if (!pcie_probe) {
		dev_err(&pdev->dev, "failed to get pcie_probe function\n");
		return -EINVAL;
	}

	ret = pcie_probe(pcie);
	if (ret) {
		dev_err(&pdev->dev, "pcie_probe fail\n");
		return -EINVAL;
	}

	ret = kirin_add_pcie_port(pp, pdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, pcie);

	kirin_pcie_rd_own_conf(pp, PCI_VENDOR_ID, 2, &vendor_id);
	kirin_pcie_rd_own_conf(pp, PCI_DEVICE_ID, 2, &dev_id);
	pcie->rc_dev = pci_get_device(vendor_id, dev_id, pcie->rc_dev);
	if (!pcie->rc_dev) {
		dev_err(&pdev->dev, "Failed to get RC device\n");
		return -EINVAL;
	}

	ret = kirin_pcie_save_rc_cfg(pcie);

	return 0;
}

#ifdef CONFIG_PM
static int kirin_pcie_resume_noirq(struct device *dev)
{
	int ret;
	struct kirin_pcie *pcie;
	int (*pcie_resume_noirq)(struct device *);

	pcie = dev_get_drvdata(dev);
	if (!pcie) {
		dev_err(dev, "Failed to get drvdata\n");
		return -EINVAL;
	}

	pcie_resume_noirq = pcie->pcie_ops->pcie_resume_noirq;
	if (!pcie_resume_noirq) {
		dev_err(dev, "failed to get pcie_resume_noirq function\n");
		return 0;
	}

	ret = pcie_resume_noirq(dev);
	if (ret)
		return ret;

	ret = kirin_pcie_establish_link(&(pcie->pp));
	if (ret)
		return ret;

	return 0;
}


static int kirin_pcie_suspend_noirq(struct device *dev)
{
	int ret;
	struct kirin_pcie *pcie;
	int (*pcie_suspend_noirq)(struct device *);

	pcie = dev_get_drvdata(dev);
	if (!pcie) {
		dev_err(dev, "Failed to get drvdata\n");
		return -EINVAL;
	}

	pcie_suspend_noirq = pcie->pcie_ops->pcie_suspend_noirq;
	if (!pcie_suspend_noirq) {
		dev_err(dev, "failed to get pcie_suspend_noirq function\n");
		return 0;
	}

	ret = pcie_suspend_noirq(dev);
	if (ret)
		return ret;

	return 0;
}

#else

#define kirin_pcie_suspend_noirq NULL
#define kirin_pcie_resume_noirq NULL

#endif

static const struct dev_pm_ops kirin_pcie_dev_pm_ops = {
	.suspend_noirq	= kirin_pcie_suspend_noirq,
	.resume_noirq		= kirin_pcie_resume_noirq,
};


static const struct of_device_id kirin_pcie_match[] = {
	{
		.compatible = "hisilicon,hikey960",
		.data = &kirin960_pcie_ops
	},
	{
		.compatible = "hisilicon,hikey970",
		.data = &kirin970_pcie_ops
	},
	{},
};
MODULE_DEVICE_TABLE(of, kirin_pcie_match);

struct platform_driver kirin_pcie_driver = {
	.probe			= kirin_pcie_probe,
	.driver			= {
		.name			= "Kirin-pcie",
		.owner			= THIS_MODULE,
		.of_match_table = kirin_pcie_match,
		.pm				=&kirin_pcie_dev_pm_ops,
	},
};

module_platform_driver(kirin_pcie_driver);

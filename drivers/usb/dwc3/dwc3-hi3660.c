/*
 * dwc3-hi3660.c
 *
 * Copyright: (C) 2008-2018 hisilicon.
 * Contact: wangbinghui<wangbinghui@hisilicon.com>
 *
 * USB vbus for Hisilicon device
 *
 * This software is available to you under a choice of one of two
 * licenses. You may choose this file to be licensed under the terms
 * of the GNU General Public License (GPL) Version 2 or the 2-clause
 * BSD license listed below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include "dwc3-hisi.h"

/*lint -e750 -esym(750,*)*/
/* clk module will round to 228M */
#define USB3OTG_ACLK_FREQ		229000000
#ifndef BIT
#define BIT(x)	(1 << (x))
#endif
#define SCTRL_SCDEEPSLEEPED				0x08
#define USB_REFCLK_ISO_EN               BIT(25)
#define PCTRL_PERI_CTRL3                0x10
#define USB_TCXO_EN						BIT(1)
#define PERI_CTRL3_MSK_START            (16)
#define SC_CLK_USB3PHY_3MUX1_SEL        BIT(25)

#define SC_SEL_ABB_BACKUP               BIT(8)
#define CLKDIV_MASK_START               (16)

#define PERI_CRG_CLKDIV21               0xFC

#define GT_CLK_ABB_BACKUP               BIT(22)
#define PERI_CRG_CLK_DIS5               0x54

#define PMC_PPLL3CTRL0                  0x048
#define PPLL3_FBDIV_START               (8)
#define PPLL3_EN                        BIT(0)
#define PPLL3_BP                        BIT(1)
#define PPLL3_LOCK                      BIT(26)

#define PMC_PPLL3CTRL1                  0x04C
#define PPLL3_INT_MOD                   BIT(24)
#define GT_CLK_PPLL3                    BIT(26)

#define PERI_CRG_CLK_EN5                0x50

#define SC_USB3PHY_ABB_GT_EN            BIT(15)
#define REF_SSP_EN                      BIT(16)
/*lint -e750 +esym(750,*)*/

static int usb3_regu_init(struct hisi_dwc3_device *hisi_dwc3)
{
	if (hisi_dwc3->is_regu_on != 0) {
		usb_dbg("ldo already opened!\n");
		return 0;
	}

	hisi_dwc3->is_regu_on = 1;

	return 0;
}

static int usb3_regu_shutdown(struct hisi_dwc3_device *hisi_dwc3)
{
	if (hisi_dwc3->is_regu_on == 0) {
		usb_dbg("regu already closed!\n");
		return 0;
	}

	hisi_dwc3->is_regu_on = 0;

	return 0;
}

static int usb3_clk_init(struct hisi_dwc3_device *hisi_dwc3)
{
	int ret;
	u32 temp;
	void __iomem *pctrl_base = hisi_dwc3->pctrl_reg_base;
	void __iomem *pericfg_base = hisi_dwc3->pericfg_reg_base;

	/* set usb aclk 240MHz to improve performance */
	ret = clk_set_rate(hisi_dwc3->gt_aclk_usb3otg, USB3OTG_ACLK_FREQ);
	if (ret)
		usb_err("usb aclk set rate failed\n");

	ret = clk_prepare_enable(hisi_dwc3->gt_aclk_usb3otg);
	if (ret) {
		usb_err("clk_prepare_enable gt_aclk_usb3otg failed\n");
		return ret;
	}

	/* usb refclk iso enable */
	writel(USB_REFCLK_ISO_EN, pericfg_base + PERI_CRG_ISODIS);

	/* enable usb_tcxo_en */
	writel(USB_TCXO_EN | (USB_TCXO_EN << PERI_CTRL3_MSK_START),
	       pctrl_base + PCTRL_PERI_CTRL3);

	/* select usbphy clk from abb */
	temp = readl(pctrl_base + PCTRL_PERI_CTRL24);
	temp &= ~SC_CLK_USB3PHY_3MUX1_SEL;
	writel(temp, pctrl_base + PCTRL_PERI_CTRL24);

	/* open clk gate */
	writel(GT_CLK_USB3OTG_REF | GT_ACLK_USB3OTG,
	       pericfg_base + PERI_CRG_CLK_EN4);

	ret = clk_prepare_enable(hisi_dwc3->clk);
	if (ret) {
		usb_err("clk_prepare_enable clk failed\n");
		return ret;
	}

	return 0;
}

static void usb3_clk_shutdown(struct hisi_dwc3_device *hisi_dwc3)
{
	u32 temp;
	void __iomem *pctrl_base = hisi_dwc3->pctrl_reg_base;
	void __iomem *pericfg_base = hisi_dwc3->pericfg_reg_base;

	writel(GT_CLK_USB3OTG_REF | GT_ACLK_USB3OTG,
	       pericfg_base + PERI_CRG_CLK_DIS4);

	temp = readl(pctrl_base + PCTRL_PERI_CTRL24);
	temp &= ~SC_CLK_USB3PHY_3MUX1_SEL;
	writel(temp, pctrl_base + PCTRL_PERI_CTRL24);

	/* disable usb_tcxo_en */
	writel(0 | (USB_TCXO_EN << PERI_CTRL3_MSK_START),
	       pctrl_base + PCTRL_PERI_CTRL3);

	clk_disable_unprepare(hisi_dwc3->clk);
	clk_disable_unprepare(hisi_dwc3->gt_aclk_usb3otg);

	msleep(20);
}

static void dwc3_release(struct hisi_dwc3_device *hisi_dwc3)
{
	u32 temp;
	void __iomem *pericfg_base = hisi_dwc3->pericfg_reg_base;
	void __iomem *otg_bc_base = hisi_dwc3->otg_bc_reg_base;

	/* dis-reset the module */
	writel(IP_RST_USB3OTG_MUX | IP_RST_USB3OTG_AHBIF | IP_RST_USB3OTG_32K,
	       pericfg_base + PERI_CRG_RSTDIS4);

	/* reset phy */
	writel(IP_RST_USB3OTGPHY_POR | IP_RST_USB3OTG,
	       pericfg_base + PERI_CRG_RSTEN4);

	/* enable phy ref clk */
	temp = readl(otg_bc_base + USBOTG3_CTRL0);
	temp |= SC_USB3PHY_ABB_GT_EN;
	writel(temp, otg_bc_base + USBOTG3_CTRL0);

	temp = readl(otg_bc_base + USBOTG3_CTRL7);
	temp |= REF_SSP_EN;
	writel(temp, otg_bc_base + USBOTG3_CTRL7);

	/* exit from IDDQ mode */
	temp = readl(otg_bc_base + USBOTG3_CTRL2);
	temp &= ~(USBOTG3CTRL2_POWERDOWN_HSP | USBOTG3CTRL2_POWERDOWN_SSP);
	writel(temp, otg_bc_base + USBOTG3_CTRL2);

	usleep_range(100, 120);

	/* dis-reset phy */
	writel(IP_RST_USB3OTGPHY_POR, pericfg_base + PERI_CRG_RSTDIS4);

	/* dis-reset controller */
	writel(IP_RST_USB3OTG, pericfg_base + PERI_CRG_RSTDIS4);

	msleep(20);

	/* fake vbus valid signal */
	temp = readl(otg_bc_base + USBOTG3_CTRL3);
	temp |= (USBOTG3_CTRL3_VBUSVLDEXT | USBOTG3_CTRL3_VBUSVLDEXTSEL);
	writel(temp, otg_bc_base + USBOTG3_CTRL3);

	usleep_range(100, 120);
}

static void dwc3_reset(struct hisi_dwc3_device *hisi_dwc3)
{
	void __iomem *pericfg_base = hisi_dwc3->pericfg_reg_base;

	writel(IP_RST_USB3OTG, pericfg_base + PERI_CRG_RSTEN4);
	writel(IP_RST_USB3OTGPHY_POR, pericfg_base + PERI_CRG_RSTEN4);
	writel(IP_RST_USB3OTG_MUX | IP_RST_USB3OTG_AHBIF | IP_RST_USB3OTG_32K,
	       pericfg_base + PERI_CRG_RSTEN4);
}

static int hi3660_usb3phy_init(struct hisi_dwc3_device *hisi_dwc3)
{
	int ret;

	usb_dbg("+\n");

	ret = usb3_regu_init(hisi_dwc3);
	if (ret)
		return ret;

	ret = usb3_clk_init(hisi_dwc3);
	if (ret)
		return ret;

	dwc3_release(hisi_dwc3);
	config_femtophy_param(hisi_dwc3);

	set_hisi_dwc3_power_flag(1);

	usb_dbg("-\n");

	return 0;
}

static int hi3660_usb3phy_shutdown(struct hisi_dwc3_device *hisi_dwc3)
{
	int ret;

	usb_dbg("+\n");

	set_hisi_dwc3_power_flag(0);

	dwc3_reset(hisi_dwc3);
	usb3_clk_shutdown(hisi_dwc3);

	ret = usb3_regu_shutdown(hisi_dwc3);
	if (ret)
		return ret;

	usb_dbg("-\n");

	return 0;
}

static struct usb3_phy_ops hi3660_phy_ops = {
	.init		= hi3660_usb3phy_init,
	.shutdown	= hi3660_usb3phy_shutdown,
};

static int dwc3_hi3660_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = hisi_dwc3_probe(pdev, &hi3660_phy_ops);
	if (ret)
		usb_err("probe failed, ret=[%d]\n", ret);

	return ret;
}

static int dwc3_hi3660_remove(struct platform_device *pdev)
{
	int ret = 0;

	ret = hisi_dwc3_remove(pdev);
	if (ret)
		usb_err("hisi_dwc3_remove failed, ret=[%d]\n", ret);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id dwc3_hi3660_match[] = {
	{ .compatible = "hisilicon,hi3660-dwc3" },
	{},
};
MODULE_DEVICE_TABLE(of, dwc3_hi3660_match);
#else
#define dwc3_hi3660_match NULL
#endif

static struct platform_driver dwc3_hi3660_driver = {
	.probe		= dwc3_hi3660_probe,
	.remove		= dwc3_hi3660_remove,
	.driver		= {
		.name	= "usb3-hi3660",
		.of_match_table = of_match_ptr(dwc3_hi3660_match),
		.pm	= HISI_DWC3_PM_OPS,
	},
};

module_platform_driver(dwc3_hi3660_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 HI3660 Glue Layer");
MODULE_AUTHOR("wangbinghui<wangbinghui@hisilicon.com>");

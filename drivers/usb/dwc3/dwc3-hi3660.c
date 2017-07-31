#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include "dwc3-hisi.h"

/*lint -e750 -esym(750,*)*/
/* clk module will round to 228M */
#define USB3OTG_ACLK_FREQ		229000000

#define SCTRL_SCDEEPSLEEPED				0x08
#define USB_REFCLK_ISO_EN               (1 << 25)
#define PCTRL_PERI_CTRL3                0x10
#define USB_TCXO_EN						(1 << 1)
#define PERI_CTRL3_MSK_START            (16)
#define SC_CLK_USB3PHY_3MUX1_SEL        (1 << 25)

#define SC_SEL_ABB_BACKUP               (1 << 8)
#define CLKDIV_MASK_START               (16)

#define PERI_CRG_CLKDIV21               0xFC

#define GT_CLK_ABB_BACKUP               (1 << 22)
#define PERI_CRG_CLK_DIS5               0x54

#define PMC_PPLL3CTRL0                  0x048
#define PPLL3_FBDIV_START               (8)
#define PPLL3_EN                        (1 << 0)
#define PPLL3_BP                        (1 << 1)
#define PPLL3_LOCK                      (1 << 26)

#define PMC_PPLL3CTRL1                  0x04C
#define PPLL3_INT_MOD                   (1 << 24)
#define GT_CLK_PPLL3                    (1 << 26)

#define PERI_CRG_CLK_EN5                0x50

#define SC_USB3PHY_ABB_GT_EN            (1 << 15)
#define REF_SSP_EN                      (1 << 16)
/*lint -e750 +esym(750,*)*/

static int usb3_regu_init(struct hisi_dwc3_device *hisi_dwc3)
{
	if (0 != hisi_dwc3->is_regu_on) {
		usb_dbg("ldo already opened!\n");
		return 0;
	}

    /*
	if (0 == hisi_dwc3->fpga_flag) {
		ret = regulator_set_mode(hisi_dwc3->usb_regu, REGULATOR_MODE_NORMAL);
		if (ret) {
			usb_err("set usb regu to normal mode failed (ret %d)\n", ret);
			return ret;
		}
	}
    */

	hisi_dwc3->is_regu_on = 1;

	return 0;
}

static int usb3_regu_shutdown(struct hisi_dwc3_device *hisi_dwc3)
{
	if (0 == hisi_dwc3->is_regu_on) {
		usb_dbg("regu already closed!\n");
		return 0;
	}

    /*
	if (0 == hisi_dwc3->fpga_flag) {
		ret = regulator_set_mode(hisi_dwc3->usb_regu, REGULATOR_MODE_IDLE);
		if (ret) {
			usb_err("set usb regu to idle mode failed (ret %d)\n", ret);
			return ret;
		}
	}
    */

	hisi_dwc3->is_regu_on = 0;

	return 0;
}

static int usb3_clk_init(struct hisi_dwc3_device *hisi_dwc3)
{
	int ret;
	uint32_t temp;
	void __iomem *pctrl_base = hisi_dwc3->pctrl_reg_base;
	void __iomem *pericfg_base = hisi_dwc3->pericfg_reg_base;

	/* set usb aclk 240MHz to improve performance */
	ret = clk_set_rate(hisi_dwc3->gt_aclk_usb3otg, USB3OTG_ACLK_FREQ);
	if (ret) {
		usb_err("usb aclk set rate failed\n");
	}

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
	uint32_t temp;
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

	msleep(10);
}

static void dwc3_release(struct hisi_dwc3_device *hisi_dwc3)
{
	uint32_t temp;
	void __iomem *pericfg_base = hisi_dwc3->pericfg_reg_base;
	void __iomem *otg_bc_base = hisi_dwc3->otg_bc_reg_base;

	/* dis-reset the module */
	writel(IP_RST_USB3OTG_MUX | IP_RST_USB3OTG_AHBIF | IP_RST_USB3OTG_32K,
			pericfg_base + PERI_CRG_RSTDIS4);

	/* reset phy */
	writel(IP_RST_USB3OTGPHY_POR | IP_RST_USB3OTG, pericfg_base + PERI_CRG_RSTEN4);

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

	udelay(100);

	/* dis-reset phy */
	writel(IP_RST_USB3OTGPHY_POR, pericfg_base + PERI_CRG_RSTDIS4);

	/* dis-reset controller */
	writel(IP_RST_USB3OTG, pericfg_base + PERI_CRG_RSTDIS4);

	msleep(10);

	/* fake vbus valid signal */
	temp = readl(otg_bc_base + USBOTG3_CTRL3);
	temp |= (USBOTG3_CTRL3_VBUSVLDEXT | USBOTG3_CTRL3_VBUSVLDEXTSEL);
	writel(temp, otg_bc_base + USBOTG3_CTRL3);

	udelay(100);
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
MODULE_LICENSE("GPL");

/*
 * PCIe host controller driver for Kirin970 Phone SoCs
 *
 * Copyright (C) 2017 Hilisicon Electronics Co., Ltd.
 *		http://www.huawei.com
 *
 * Author: Yao Chen <chenyao11@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pcie-kirin.h"

#define AXI_CLK_FREQ        207500000

#define EYEPARAM_NOCFG 0xFFFFFFFF
#define RAWLANEN_DIG_PCS_XF_TX_OVRD_IN_1 0x3001
#define SUP_DIG_LVL_OVRD_IN 0xf
#define LANEN_DIG_ASIC_TX_OVRD_IN_1 0x1002
#define LANEN_DIG_ASIC_TX_OVRD_IN_2 0x1003

/* kirin970 pciephy register */
#define SOC_PCIEPHY_MMC1PLL_CTRL1  0xc04
#define SOC_PCIEPHY_MMC1PLL_CTRL16 0xC40
#define SOC_PCIEPHY_MMC1PLL_CTRL17 0xC44
#define SOC_PCIEPHY_MMC1PLL_CTRL20 0xC50
#define SOC_PCIEPHY_MMC1PLL_CTRL21 0xC54
#define SOC_PCIEPHY_MMC1PLL_STAT0  0xE00

#define CRGPERIPH_PEREN12   0x470
#define CRGPERIPH_PERDIS12  0x474
#define CRGPERIPH_PCIECTRL0 0x800

/* define ie,oe cfg */
#define IO_IE_EN_HARD_BYPASS         (0x1 << 27)
#define IO_OE_EN_HARD_BYPASS         (0x1 << 11)
#define IO_HARD_CTRL_DEBOUNCE_BYPASS (0x1 << 10)
#define IO_OE_GT_MODE                (0x2 << 7)
#define DEBOUNCE_WAITCFG_IN          (0xf << 20)
#define DEBOUNCE_WAITCFG_OUT         (0xf << 13)

/* noc power domain */
#define NOC_POWER_IDLEREQ_1 0x38c
#define NOC_POWER_IDLE_1    0x394
#define NOC_PW_MASK         0x10000
#define NOC_PW_SET_BIT      0x1

static inline void kirin970_phy_writel(struct kirin_pcie *pcie, u32 val, u32 reg)
{
	writel(val, pcie->phy_base + 0x40000 + reg);
}

static inline u32 kirin970_phy_readl(struct kirin_pcie *pcie, u32 reg)
{
	return readl(pcie->phy_base + 0x40000 + reg);
}

static int32_t kirin970_pcie_pinctrl_init(struct kirin_pcie *pcie,
				  struct platform_device *pdev)
{
	int ret;
	struct pinctrl *p;
	struct pinctrl_state *pinctrl_def;

	p = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(p))
		return PTR_ERR(p);

	pinctrl_def = pinctrl_lookup_state(p, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(pinctrl_def)) {
		dev_err(&pdev->dev, "Failed to get defult pinctrl state\n");
		return PTR_ERR(pinctrl_def);
	}

	ret = pinctrl_select_state(p, pinctrl_def);
	if (ret) {
		dev_err(&pdev->dev, "Failed to select defult pinctrl state\n");
		return ret;
	}

	return 0;
}

static int32_t kirin970_pcie_get_clk(struct kirin_pcie *pcie,
				  struct platform_device *pdev)
{
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

static int32_t kirin970_pcie_get_resource(struct pcie_port *pp,
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
		syscon_regmap_lookup_by_compatible("hisilicon,kirin970-crgctrl");
	if (IS_ERR(pcie->crgctrl))
		return PTR_ERR(pcie->crgctrl);

	pcie->sysctrl =
		syscon_regmap_lookup_by_compatible("hisilicon,kirin970-sctrl");
	if (IS_ERR(pcie->sysctrl))
		return PTR_ERR(pcie->sysctrl);

	pcie->pmctrl =
		syscon_regmap_lookup_by_compatible("hisilicon,kirin970-pmctrl");
	if (IS_ERR(pcie->pmctrl))
		return PTR_ERR(pcie->pmctrl);

	return 0;
}

void kirin_pcie_get_eyeparam(struct kirin_pcie *pcie, struct platform_device *pdev)
{
	int i;
	struct device_node *np;

	np = pdev->dev.of_node;

	if (of_property_read_u32_array(np, "eye_param", pcie->eye_param, 5)) {
		for (i = 0; i < 5; i++)
		pcie->eye_param[i] = EYEPARAM_NOCFG;
	}

	dev_err(&pdev->dev, "eye_param_vboost = [0x%x]\n", pcie->eye_param[0]);
	dev_err(&pdev->dev, "eye_param_iboost = [0x%x]\n", pcie->eye_param[1]);
	dev_err(&pdev->dev, "eye_param_pre = [0x%x]\n", pcie->eye_param[2]);
	dev_err(&pdev->dev, "eye_param_post = [0x%x]\n", pcie->eye_param[3]);
	dev_err(&pdev->dev, "eye_param_main = [0x%x]\n", pcie->eye_param[4]);
}

static void set_phy_eye_param(struct kirin_pcie *pcie)
{
	u32 val;

	val = kirin_natural_phy_readl(pcie, RAWLANEN_DIG_PCS_XF_TX_OVRD_IN_1);

	if (pcie->eye_param[1] != EYEPARAM_NOCFG) {
		val &= (~0xf00);
		val |= (pcie->eye_param[1] << 8) | (0x1 << 12);
	}
	kirin_natural_phy_writel(pcie, val, RAWLANEN_DIG_PCS_XF_TX_OVRD_IN_1);

	val = kirin_natural_phy_readl(pcie, LANEN_DIG_ASIC_TX_OVRD_IN_2);
	val &= (~0x1FBF);
	if (pcie->eye_param[2] != EYEPARAM_NOCFG)
		val |= (pcie->eye_param[2]<< 0) | (0x1 << 6);

	if (pcie->eye_param[3] != EYEPARAM_NOCFG)
		val |= (pcie->eye_param[3] << 7) | (0x1 << 13);

	kirin_natural_phy_writel(pcie, val, LANEN_DIG_ASIC_TX_OVRD_IN_2);

	val = kirin_natural_phy_readl(pcie, SUP_DIG_LVL_OVRD_IN);
	if (pcie->eye_param[0] != EYEPARAM_NOCFG) {
		val &= (~0x1C0);
		val |= (pcie->eye_param[0] << 6) | (0x1 << 9);
	}
	kirin_natural_phy_writel(pcie, val, SUP_DIG_LVL_OVRD_IN);

	val = kirin_natural_phy_readl(pcie, LANEN_DIG_ASIC_TX_OVRD_IN_1);
	if (pcie->eye_param[4] != EYEPARAM_NOCFG) {
		val &= (~0x7E00);
		val |= (pcie->eye_param[4] << 9) | (0x1 << 15);
	}
	kirin_natural_phy_writel(pcie, val, LANEN_DIG_ASIC_TX_OVRD_IN_1);
}

static int kirin970_pcie_pclk_ctrl(struct kirin_pcie *pcie, bool enable)
{
	int ret = 0;

	if (!enable)
		goto close_clk;

	ret = clk_prepare_enable(pcie->apb_phy_clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(pcie->apb_sys_clk);
	if (ret)
		goto apb_sys_fail;

	return 0;

close_clk:
	clk_disable_unprepare(pcie->apb_sys_clk);
apb_sys_fail:
	clk_disable_unprepare(pcie->phy_ref_clk);

	return ret;
}

static int kirin970_pcie_clk_ctrl(struct clk *clk, int clk_on)
{
	int ret = 0;

	if (clk_on) {
		ret = clk_prepare_enable(clk);
		if (ret)
			return ret;
	} else {
		clk_disable_unprepare(clk);
	}

	return ret;
}

static void kirin970_pcie_natural_cfg(struct kirin_pcie *pcie)
{
	u32 val;

	/* change 2p mem_ctrl */
	kirin_elb_writel(pcie, 0x02605550, SOC_PCIECTRL_CTRL20_ADDR);

	/* pull up sys_aux_pwr_det */
	val = kirin_elb_readl(pcie, SOC_PCIECTRL_CTRL7_ADDR);
	val |= (0x1 << 10);
	kirin_elb_writel(pcie, val, SOC_PCIECTRL_CTRL7_ADDR);

	/* output, pull down */
	val = kirin_elb_readl(pcie, SOC_PCIECTRL_CTRL12_ADDR);
	val &= ~(0x3 << 2);
	val |= (0x1 << 1);
	val &= ~(0x1 << 0);
	kirin_elb_writel(pcie, val, SOC_PCIECTRL_CTRL12_ADDR);

	/* Handle phy_reset and lane0_reset to HW */
	val = kirin_phy_readl(pcie, SOC_PCIEPHY_CTRL1_ADDR);
	val |= PCIEPHY_RESET_BIT;
	val &= ~PCIEPHY_PIPE_LINE0_RESET_BIT;
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_CTRL1_ADDR);

	/* fix chip bug: TxDetectRx fail */
	val = kirin_phy_readl(pcie, SOC_PCIEPHY_CTRL38_ADDR);
	val |= (0x1 << 2);
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_CTRL38_ADDR);

}

static void kirin970_pcie_pll_init(struct kirin_pcie *pcie)
{
	u32 val;

	/* choose FNPLL */
	val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL1);
	val |= (0x1 << 27);
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL1);

	val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL16);
	val &= 0xF000FFFF;
	/* fnpll fbdiv = 0xD0 */
	val |= (0xd0 << 16);
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL16);

	val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL17);
	val &= 0xFF000000;
	/* fnpll fracdiv = 0x555555 */
	val |= (0x555555 << 0);
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL17);

	val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL20);
	val &= 0xF5FF88FF;
	/* fnpll dll_en = 0x1 */
	val |= (0x1 << 27);
	/* fnpll postdiv1 = 0x5 */
	val |= (0x5 << 8);
	/* fnpll postdiv2 = 0x4 */
	val |= (0x4 << 12);
	/* fnpll pll_mode = 0x0 */
	val &= ~(0x1 << 25);
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL20);

	kirin_phy_writel(pcie, 0x20, SOC_PCIEPHY_MMC1PLL_CTRL21);
}

static int kirin970_pcie_pll_ctrl(struct kirin_pcie *pcie, bool enable)
{
	u32 val;
	int time = 200;

	if (enable) {
		/* pd = 0 */
		val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL16);
		val &= ~(0x1 << 0);
		kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL16);

		val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_STAT0);

		/* choose FNPLL */
		while (!(val & 0x10)) {
			if (!time) {
				dev_err(pcie->pp.dev, "wait for pll_lock timeout\n");
				return -1;
			}
			time --;
			udelay(1);
			val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_STAT0);
		}

		/* pciepll_bp = 0 */
		val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL20);
		val &= ~(0x1 << 16);
		kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL20);

	} else {
		/* pd = 1 */
		val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL16);
		val |= (0x1 << 0);
		kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL16);

		/* pciepll_bp = 1 */
		val = kirin_phy_readl(pcie, SOC_PCIEPHY_MMC1PLL_CTRL20);
		val |= (0x1 << 16);
		kirin_phy_writel(pcie, val, SOC_PCIEPHY_MMC1PLL_CTRL20);
	}

	 return 0;
}

static void kirin970_pcie_hp_debounce_gt(struct kirin_pcie *pcie, bool open)
{
	if (open)
		/* gt_clk_pcie_hp/gt_clk_pcie_debounce open */
		regmap_write(pcie->crgctrl, CRGPERIPH_PEREN12, 0x9000);
	else
		/* gt_clk_pcie_hp/gt_clk_pcie_debounce close */
		regmap_write(pcie->crgctrl, CRGPERIPH_PERDIS12, 0x9000);
}

static void kirin970_pcie_phyref_gt(struct kirin_pcie *pcie, bool open)
{
	unsigned int val;

	regmap_read(pcie->crgctrl, CRGPERIPH_PCIECTRL0, &val);

	if (open)
		val &= ~(0x1 << 1); //enable hard gt mode
	else
		val |= (0x1 << 1); //disable hard gt mode

	regmap_write(pcie->crgctrl, CRGPERIPH_PCIECTRL0, val);

	/* disable soft gt mode */
	regmap_write(pcie->crgctrl, CRGPERIPH_PERDIS12, 0x4000);
}

static void kirin970_pcie_oe_ctrl(struct kirin_pcie *pcie, bool en_flag)
{
	unsigned int val;

	regmap_read(pcie->crgctrl , CRGPERIPH_PCIECTRL0, &val);

	/* set ie cfg */
	val |= IO_IE_EN_HARD_BYPASS;

	/* set oe cfg */
	val &= ~IO_HARD_CTRL_DEBOUNCE_BYPASS;

	/* set phy_debounce in&out time */
	val |= (DEBOUNCE_WAITCFG_IN | DEBOUNCE_WAITCFG_OUT);

	/* select oe_gt_mode */
	val |= IO_OE_GT_MODE;

	if (en_flag)
		val &= ~IO_OE_EN_HARD_BYPASS;
	else
		val |= IO_OE_EN_HARD_BYPASS;

	regmap_write(pcie->crgctrl, CRGPERIPH_PCIECTRL0, val);
}

static void kirin970_pcie_ioref_gt(struct kirin_pcie *pcie, bool open)
{
	unsigned int val;

	if (open) {
		kirin_elb_writel(pcie, 0x20000070, SOC_PCIECTRL_CTRL21_ADDR);

		kirin970_pcie_oe_ctrl(pcie, true);

		/* en hard gt mode */
		regmap_read(pcie->crgctrl, CRGPERIPH_PCIECTRL0, &val);
		val &= ~(0x1 << 0);
		regmap_write(pcie->crgctrl, CRGPERIPH_PCIECTRL0, val);

		/* disable soft gt mode */
		regmap_write(pcie->crgctrl, CRGPERIPH_PERDIS12, 0x2000);

	} else {
		/* disable hard gt mode */
		regmap_read(pcie->crgctrl, CRGPERIPH_PCIECTRL0, &val);
		val |= (0x1 << 0);
		regmap_write(pcie->crgctrl, CRGPERIPH_PCIECTRL0, val);

		/* disable soft gt mode */
		regmap_write(pcie->crgctrl, CRGPERIPH_PERDIS12, 0x2000);

		kirin970_pcie_oe_ctrl(pcie, false);
       }
}

static int kirin970_pcie_allclk_ctrl(struct kirin_pcie *pcie, bool clk_on)
{
	u32 val;
	int ret = 0;

	if (!clk_on)
		goto ALL_CLOSE;

	/* choose 100MHz clk src: Bit[8]==1 pad, Bit[8]==0 pll */
	val = kirin_phy_readl(pcie, SOC_PCIEPHY_CTRL1_ADDR);
	val &= ~(0x1 << 8);
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_CTRL1_ADDR);

	kirin970_pcie_pll_init(pcie);

	ret = kirin970_pcie_pll_ctrl(pcie, true);
	if (ret) {
		dev_err(pcie->pp.dev, "Failed to enable pll\n");
		return -1;
	}
	kirin970_pcie_hp_debounce_gt(pcie, true);
	kirin970_pcie_phyref_gt(pcie, true);
	kirin970_pcie_ioref_gt(pcie, true);

	ret = clk_set_rate(pcie->pcie_aclk, AXI_CLK_FREQ);
	if (ret) {
		dev_err(pcie->pp.dev, "Failed to set rate\n");
		goto GT_CLOSE;
	}

	ret = kirin970_pcie_clk_ctrl(pcie->pcie_aclk, true);
	if (ret) {
		dev_err(pcie->pp.dev, "Failed to enable pcie_aclk\n");
		goto GT_CLOSE;
	}

	ret = kirin970_pcie_clk_ctrl(pcie->pcie_aux_clk, true);
	if (ret) {
		dev_err(pcie->pp.dev, "Failed to enable pcie_aux_clk\n");
		goto AUX_CLK_FAIL;
	}

	return 0;

ALL_CLOSE:
	kirin970_pcie_clk_ctrl(pcie->pcie_aux_clk, false);
AUX_CLK_FAIL:
	kirin970_pcie_clk_ctrl(pcie->pcie_aclk, false);
GT_CLOSE:
	kirin970_pcie_ioref_gt(pcie, false);
	kirin970_pcie_phyref_gt(pcie, false);
	kirin970_pcie_hp_debounce_gt(pcie, false);

	kirin970_pcie_pll_ctrl(pcie, false);

	return ret;
}

static bool is_pipe_clk_stable(struct kirin_pcie *pcie)
{
	u32 val;
	u32 time = 100;
	u32 pipe_clk_stable = 0x1 << 19;

	val = kirin_phy_readl(pcie, SOC_PCIEPHY_STATE0_ADDR);
	while (val & pipe_clk_stable) {
		mdelay(1);
		if (time == 0) {
			dev_err(pcie->pp.dev, "PIPE clk is not stable\n");
			return false;
		}
		time--;
		val = kirin_phy_readl(pcie, SOC_PCIEPHY_STATE0_ADDR);
	}

	return true;
}

static int kirin970_pcie_noc_power(struct kirin_pcie *pcie, bool enable)
{
	u32 time = 100;
	unsigned int val = NOC_PW_MASK;
	int rst;

	if (enable)
		val = NOC_PW_MASK | NOC_PW_SET_BIT;
	else
		val = NOC_PW_MASK;
	rst = enable ? 1 : 0;

	regmap_write(pcie->pmctrl, NOC_POWER_IDLEREQ_1, val);

	time = 100;
	regmap_read(pcie->pmctrl, NOC_POWER_IDLE_1, &val);
	while((val & NOC_PW_SET_BIT) != rst) {
		udelay(10);
		if (!time) {
			dev_err(pcie->pp.dev, "Failed to reverse noc power-status\n");
			return -1;
		}
		time--;
		regmap_read(pcie->pmctrl, NOC_POWER_IDLE_1, &val);
	}

	return 0;
}

static int kirin970_pcie_perst_cfg(struct kirin_pcie *pcie, int pull_up)
{
	int ret;

	if (pull_up)
		usleep_range(21000, 23000);

	ret = gpio_direction_output(pcie->gpio_id_reset[0], pull_up);
	if (ret)
		dev_err(pcie->pp.dev, "Failed to pulse switch perst signal\n");

	ret = gpio_direction_output(pcie->gpio_id_reset[1], pull_up);
	if (ret)
		dev_err(pcie->pp.dev, "Failed to pulse eth perst signal\n");

	ret = gpio_direction_output(pcie->gpio_id_reset[2], pull_up);
	if (ret)
		dev_err(pcie->pp.dev, "Failed to pulse m.2 perst signal\n");

	ret = gpio_direction_output(pcie->gpio_id_reset[3], pull_up);
	if (ret)
		dev_err(pcie->pp.dev, "Failed to pulse mini1 perst signal\n");

	usleep_range(10000, 11000);

	return ret;
}

static int kirin970_pcie_clkreq_cfg(struct kirin_pcie *pcie, int pull_up)
{
	int ret;

	ret = gpio_direction_output(pcie->gpio_id_clkreq[0], pull_up);
	if (ret)
		dev_err(pcie->pp.dev, "Failed to pulse eth clkreq signal\n");

	ret = gpio_direction_output(pcie->gpio_id_clkreq[1], pull_up);
	if (ret)
		dev_err(pcie->pp.dev, "Failed to pulse m.2 clkreq signal\n");

	ret = gpio_direction_output(pcie->gpio_id_clkreq[2], pull_up);
	if (ret)
		dev_err(pcie->pp.dev, "Failed to pulse mini1 clkreq signal\n");

	return ret;
}

int kirin970_pcie_turn_on(struct kirin_pcie *pcie)
{
	int ret;
	u32 val;

	ret = kirin970_pcie_clkreq_cfg(pcie, false);
	if (ret)
		return ret;

	/* pull downphy ISO */
	regmap_write(pcie->sysctrl, 0x44, 0x20);

	/* enable PCIe sys&phy pclk */
	ret = kirin970_pcie_pclk_ctrl(pcie, true);
	if (ret)
		return ret;

	/* deasset PCIeCtrl&PCIePHY */
	regmap_write(pcie->crgctrl, 0x88, 0x8c000000);

	kirin970_pcie_natural_cfg(pcie);

	if (kirin970_pcie_allclk_ctrl(pcie, true))
		goto PCLK_CLOSE;

	/* pull down phy_test_powerdown signal */
	val = kirin_phy_readl(pcie, SOC_PCIEPHY_CTRL0_ADDR);
	val &= ~(0x1 << 22);
	kirin_phy_writel(pcie, val, SOC_PCIEPHY_CTRL0_ADDR);

	/* deassert controller perst_n */
	val = kirin_elb_readl(pcie, SOC_PCIECTRL_CTRL12_ADDR);
	val |= (0x1 << 2);
	kirin_elb_writel(pcie, val, SOC_PCIECTRL_CTRL12_ADDR);
	udelay(10);

	kirin970_pcie_perst_cfg(pcie, true);

	if (!is_pipe_clk_stable(pcie))
		goto ALLCLK_CLOSE;

	set_phy_eye_param(pcie);

	if (kirin970_pcie_noc_power(pcie, false))
		goto ALLCLK_CLOSE;

	return 0;

ALLCLK_CLOSE:
	kirin970_pcie_allclk_ctrl(pcie, false);
PCLK_CLOSE:
	kirin970_pcie_pclk_ctrl(pcie, false);
	return -1;
}

int kirin970_pcie_turn_off(struct kirin_pcie *pcie)
{
	u32 val;
	int ret;

	ret = kirin970_pcie_noc_power(pcie, true);
	if (ret)
		return ret;

	kirin970_pcie_perst_cfg(pcie, false);

	val = kirin_elb_readl(pcie, SOC_PCIECTRL_CTRL12_ADDR);
	val &= ~(0x1 << 2);
	kirin_elb_writel(pcie, val, SOC_PCIECTRL_CTRL12_ADDR);

	kirin970_pcie_allclk_ctrl(pcie, false);

	regmap_write(pcie->crgctrl, 0x84, 0x8c000000);

	kirin970_pcie_pclk_ctrl(pcie, false);

	regmap_write(pcie->sysctrl, 0x40, 0x20);

	ret = kirin970_pcie_clkreq_cfg(pcie, true);
	if (ret)
		return ret;

	return 0;
}

int kirin970_pcie_power_on(struct kirin_pcie *pcie, bool on)
{
	if (on)
		return kirin970_pcie_turn_on(pcie);
	else
		return kirin970_pcie_turn_off(pcie);
}

static int kirin970_pcie_probe(struct kirin_pcie *pcie)
{
	struct platform_device *pdev;
	struct pcie_port *pp;
	int ret;

	pp = &pcie->pp;
	pdev = to_platform_device(pp->dev);
	
	ret = kirin970_pcie_get_clk(pcie, pdev);
	if (ret != 0)
		return -ENODEV;	

	ret = kirin970_pcie_get_resource(pp, pdev);
	if (ret != 0)
		return -ENODEV;
	
	kirin_pcie_get_eyeparam(pcie, pdev);
	
	pcie->gpio_id_reset[0] = of_get_named_gpio(pdev->dev.of_node,
			"switch,reset-gpios", 0);
	pcie->gpio_id_reset[1] = of_get_named_gpio(pdev->dev.of_node,
			"eth,reset-gpios", 0);
	pcie->gpio_id_reset[2] = of_get_named_gpio(pdev->dev.of_node,
			"m_2,reset-gpios", 0);
	pcie->gpio_id_reset[3] = of_get_named_gpio(pdev->dev.of_node,
			"mini1,reset-gpios", 0);
	
	if (pcie->gpio_id_reset[0] < 0)
		return -ENODEV;
	if (pcie->gpio_id_reset[1] < 0)
		return -ENODEV;
	if (pcie->gpio_id_reset[2] < 0)
		return -ENODEV;
	if (pcie->gpio_id_reset[3] < 0)
		return -ENODEV;
	
	if (gpio_request((unsigned int)pcie->gpio_id_reset[0], "pcie_switch_reset"))
		return -EINVAL;
	if (gpio_request((unsigned int)pcie->gpio_id_reset[1], "pcie_eth_reset"))
		return -EINVAL;
	if (gpio_request((unsigned int)pcie->gpio_id_reset[2], "pcie_m_2_reset"))
		return -EINVAL;
	if (gpio_request((unsigned int)pcie->gpio_id_reset[3], "pcie_mini1_reset"))
		return -EINVAL;
	
	pcie->ldo33 = devm_regulator_get(pp->dev, "ldo33");
	if(IS_ERR_OR_NULL(pcie->ldo33))
		return PTR_ERR(pcie->ldo33);

	ret = regulator_enable(pcie->ldo33);
	if (ret) {
		dev_err(pp->dev, "Failed to enable ldo33\n");
		return ret;
	}

	ret = kirin970_pcie_pinctrl_init(pcie, pdev);
	if (ret != 0)
		return -ENODEV;

	pcie->gpio_id_clkreq[0] = of_get_named_gpio(pdev->dev.of_node,
			"eth,clkreq-gpios", 0);
	pcie->gpio_id_clkreq[1] = of_get_named_gpio(pdev->dev.of_node,
			"m_2,clkreq-gpios", 0);
	pcie->gpio_id_clkreq[2] = of_get_named_gpio(pdev->dev.of_node,
			"mini1,clkreq-gpios", 0);

	if ((pcie->gpio_id_clkreq[0] < 0) || (pcie->gpio_id_clkreq[1] < 0)
		|| (pcie->gpio_id_clkreq[2] < 0))
		return -ENODEV;

	if (gpio_request((unsigned int)pcie->gpio_id_clkreq[0], "pcie_eth_clkreq"))
		return -EINVAL;
	if (gpio_request((unsigned int)pcie->gpio_id_clkreq[1], "pcie_m_2_clkreq"))
		return -EINVAL;
	if (gpio_request((unsigned int)pcie->gpio_id_clkreq[2], "pcie_mini1_clkreq"))
		return -EINVAL;

	ret = kirin970_pcie_power_on(pcie, true);
	if (ret)
		return ret;

	return 0;
}

static int kirin970_pcie_resume_noirq(struct device *dev)
{
	struct pci_dev *rc_dev;
	struct pcie_port *pp;
	struct kirin_pcie *pcie;

	pcie = dev_get_drvdata(dev);
	if (!pcie) {
		dev_err(dev, "Failed to get drvdata\n");
		return -EINVAL;
	}
	pp = &pcie->pp;
	rc_dev = pcie->rc_dev;

	if (kirin970_pcie_power_on(pcie, true)) {
		dev_err(dev, "Failed to power on\n");
		return -EINVAL;
	}

	if (rc_dev)
		kirin_pcie_restore_rc_cfg(pcie);

	return 0;
}


static int kirin970_pcie_suspend_noirq(struct device *dev)
{
	struct kirin_pcie *pcie;
	struct pci_dev *rc_dev;
	struct pcie_port *pp;

	pcie = dev_get_drvdata(dev);
	if (!pcie) {
		dev_err(dev, "Failed to get drvdata\n");
		return -EINVAL;
	}
	rc_dev = pcie->rc_dev;
	pp = &pcie->pp;

	if (kirin970_pcie_power_on(pcie, false)) {
		dev_err(dev, "Failed to power off\n");
		return -EINVAL;
	}

	return 0;
}

const struct kirin_pcie_ops kirin970_pcie_ops = {
	.pcie_probe = kirin970_pcie_probe,
	.kirin_phy_writel = kirin970_phy_writel,
	.kirin_phy_readl = kirin970_phy_readl,
	.pcie_suspend_noirq = kirin970_pcie_suspend_noirq,
	.pcie_resume_noirq = kirin970_pcie_resume_noirq,
};

 EXPORT_SYMBOL_GPL(kirin970_pcie_ops);

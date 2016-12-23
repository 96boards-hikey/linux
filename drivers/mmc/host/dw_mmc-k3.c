/*
 * Copyright (c) 2013 Linaro Ltd.
 * Copyright (c) 2013 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mmc/host.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

/*
 * hi6220 sd only support io voltage 1.8v and 3v
 * Also need config AO_SCTRL_SEL18 accordingly
 */
#define AO_SCTRL_SEL18		BIT(10)
#define AO_SCTRL_CTRL3		0x40C

#include <linux/delay.h>

#define DWMMC_SD_ID 1
#define DWMMC_SDIO_ID 2

#define BIT_VOLT_OFFSET         (0x314)
#define BIT_VOLT_VALUE_18       (0x4)

#define BIT_RST_SD              (1<<18)
#define PERI_CRG_RSTDIS4  (0x94)
#define PERI_CRG_RSTEN4   (0x90)
#define BIT_RST_SDIO    (1<<20)

struct k3_priv {
	struct regmap	*reg;
};

static void __iomem *sys_base;

static void dw_mci_k3_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	int ret;

	ret = clk_set_rate(host->ciu_clk, ios->clock);
	if (ret)
		dev_warn(host->dev, "failed to set rate %uHz\n", ios->clock);

	host->bus_hz = clk_get_rate(host->ciu_clk);
}

static const struct dw_mci_drv_data k3_drv_data = {
	.set_ios		= dw_mci_k3_set_ios,
};

static int dw_mci_hi6220_parse_dt(struct dw_mci *host)
{
	struct k3_priv *priv;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->reg = syscon_regmap_lookup_by_phandle(host->dev->of_node,
					 "hisilicon,peripheral-syscon");
	if (IS_ERR(priv->reg))
		priv->reg = NULL;

	host->priv = priv;
	return 0;
}

static int dw_mci_hi6220_switch_voltage(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct k3_priv *priv;
	struct dw_mci *host;
	int min_uv, max_uv;
	int ret;

	host = slot->host;
	priv = host->priv;

	if (!priv || !priv->reg)
		return 0;

	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		ret = regmap_update_bits(priv->reg, AO_SCTRL_CTRL3,
					 AO_SCTRL_SEL18, 0);
		min_uv = 3000000;
		max_uv = 3000000;
	} else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
		ret = regmap_update_bits(priv->reg, AO_SCTRL_CTRL3,
					 AO_SCTRL_SEL18, AO_SCTRL_SEL18);
		min_uv = 1800000;
		max_uv = 1800000;
	} else {
		dev_dbg(host->dev, "voltage not supported\n");
		return -EINVAL;
	}

	if (ret) {
		dev_dbg(host->dev, "switch voltage failed\n");
		return ret;
	}

	if (IS_ERR_OR_NULL(mmc->supply.vqmmc))
		return 0;

	ret = regulator_set_voltage(mmc->supply.vqmmc, min_uv, max_uv);
	if (ret) {
		dev_dbg(host->dev, "Regulator set error %d: %d - %d\n",
				 ret, min_uv, max_uv);
		return ret;
	}

	return 0;
}

static void dw_mci_hi6220_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	int ret;
	unsigned int clock;

	clock = (ios->clock <= 25000000) ? 25000000 : ios->clock;

	ret = clk_set_rate(host->biu_clk, clock);
	if (ret)
		dev_warn(host->dev, "failed to set rate %uHz\n", clock);

	host->bus_hz = clk_get_rate(host->biu_clk);
}

static const struct dw_mci_drv_data hi6220_data = {
	.switch_voltage		= dw_mci_hi6220_switch_voltage,
	.set_ios		= dw_mci_hi6220_set_ios,
	.parse_dt		= dw_mci_hi6220_parse_dt,
};

static int dw_mci_hs_get_resource(void)
{
	struct device_node *np = NULL;

	if (!sys_base) {
		np = of_find_compatible_node(NULL, NULL, "hisilicon,hi3660-sctrl");
		if (!np) {
			printk("can't find sysctrl!\n");
			return -1;
		}

		sys_base = of_iomap(np, 0);
		if (!sys_base) {
			printk("sysctrl iomap error!\n");
			return -1;
		}
	}

	return 0;
}

void sdcard_set_pmic_reg(void)
{
	unsigned char data = 0;
	void __iomem *iomem = ioremap(0xfff34000, 0x1000);

	data = readb(iomem + (0x78 << 2)) | (1 << 1);
	writeb(data, iomem + (0x78 << 2));
	data = readb(iomem + (0x79 << 2)) & ~(0x7) | 6;
	writeb(data, iomem + (0x79 << 2));

	data = readb(iomem + (0x6A << 2)) | (1 << 1);
	writeb(data, iomem + (0x6A << 2));
	data = readb(iomem + (0x6B << 2)) & ~(0x7) | 5;
	writeb(data, iomem + (0x6B << 2));
	iounmap(iomem);
}

int dw_mci_hi3660_init(struct dw_mci *host)
{
	dw_mci_hs_get_resource();
	sdcard_set_pmic_reg();

	return 0;
}

int dw_mci_hi3660_setup_clock(struct dw_mci *host)
{
        /* set threshold to 512 bytes */
        mci_writel(host, CDTHRCTL, 0x02000001);
	return 0;
}

static void dw_mci_hi3660_prepare_command(struct dw_mci *host, u32 *cmdr)
{
	*cmdr |= SDMMC_CMD_USE_HOLD_REG;
}

static int dw_mci_set_sel18(bool set)
{
	u32 reg;

	reg = readl(sys_base + BIT_VOLT_OFFSET);
	if (set)
		reg |= BIT_VOLT_VALUE_18;
	else
		reg &= ~BIT_VOLT_VALUE_18;
	writel(reg, sys_base + BIT_VOLT_OFFSET);

	return 0;
}

void dw_mci_hi3660_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	int ret;
	unsigned int clock;
	switch (ios->power_mode) {
		case MMC_POWER_OFF:
			break;
		case MMC_POWER_UP:
			/* for sdcard */
			dw_mci_set_sel18(0);
			/* Wait for 5ms */
			usleep_range(5000, 5500);
			break;
		case MMC_POWER_ON:
			break;
		default:
			dev_info(host->dev, "unknown power supply mode\n");
			break;
	}

	clock = 50000000;

	ret = clk_set_rate(host->ciu_clk, clock);
	if (ret)
		dev_err(host->dev, "failed to set rate %uHz\n", clock);

	host->bus_hz = 50000000;
}

static const struct dw_mci_drv_data hi3660_data = {
	.init = dw_mci_hi3660_init,
	.setup_clock = dw_mci_hi3660_setup_clock,
	.prepare_command = dw_mci_hi3660_prepare_command,
	.set_ios = dw_mci_hi3660_set_ios,
};

static const struct of_device_id dw_mci_k3_match[] = {
	{ .compatible = "hisilicon,hi3660-dw-mshc", .data = &hi3660_data, },
	{ .compatible = "hisilicon,hi4511-dw-mshc", .data = &k3_drv_data, },
	{ .compatible = "hisilicon,hi6220-dw-mshc", .data = &hi6220_data, },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_k3_match);

static int dw_mci_k3_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data;
	const struct of_device_id *match;
	struct reset_control	*rst;

	rst = devm_reset_control_get(&pdev->dev, NULL);
	if (!IS_ERR(rst))
		reset_control_reset(rst);

	match = of_match_node(dw_mci_k3_match, pdev->dev.of_node);
	drv_data = match->data;

	return dw_mci_pltfm_register(pdev, drv_data);
}

#ifdef CONFIG_PM_SLEEP
static int dw_mci_k3_suspend(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	int ret;

	ret = dw_mci_suspend(host);
	if (!ret)
		clk_disable_unprepare(host->ciu_clk);

	return ret;
}

static int dw_mci_k3_resume(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(host->ciu_clk);
	if (ret) {
		dev_err(host->dev, "failed to enable ciu clock\n");
		return ret;
	}

	return dw_mci_resume(host);
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(dw_mci_k3_pmops, dw_mci_k3_suspend, dw_mci_k3_resume);

static struct platform_driver dw_mci_k3_pltfm_driver = {
	.probe		= dw_mci_k3_probe,
	.remove		= dw_mci_pltfm_remove,
	.driver		= {
		.name		= "dwmmc_k3",
		.of_match_table	= dw_mci_k3_match,
		.pm		= &dw_mci_k3_pmops,
	},
};

module_platform_driver(dw_mci_k3_pltfm_driver);

MODULE_DESCRIPTION("K3 Specific DW-MSHC Driver Extension");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dwmmc_k3");

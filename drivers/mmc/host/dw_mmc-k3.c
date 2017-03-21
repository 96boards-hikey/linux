/*
 * Copyright (c) 2013 Linaro Ltd.
 * Copyright (c) 2013 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/mmc/host.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

/*
 * hi6220 sd only support io voltage 1.8v and 3v
 * Also need config AO_SCTRL_SEL18 accordingly
 */
#define AO_SCTRL_SEL18		BIT(10)
#define AO_SCTRL_CTRL3		0x40C

#define DWMMC_SD_ID   1
#define DWMMC_SDIO_ID 2

#define SOC_SCTRL_SCPERCTRL5    (0x314)
#define SDCARD_IO_SEL18         BIT(2)

#define GENCLK_DIV (7)

#define GPIO_CLK_ENABLE                 BIT(16)
#define GPIO_CLK_DIV(x)                 (((x) & 0xf) << 8)
#define GPIO_USE_SAMPLE_DLY(x)          (((x) & 0x1) << 13)
#define UHS_REG_EXT_SAMPLE_PHASE(x)     (((x) & 0x1f) << 16)
#define UHS_REG_EXT_SAMPLE_DLY(x)       (((x) & 0x1f) << 26)
#define UHS_REG_EXT_SAMPLE_DRVPHASE(x)  (((x) & 0x1f) << 21)
#define SDMMC_UHS_REG_EXT_VALUE(x, y, z) (UHS_REG_EXT_SAMPLE_PHASE(x) |\
					  UHS_REG_EXT_SAMPLE_DLY(y) |\
					  UHS_REG_EXT_SAMPLE_DRVPHASE(z))
#define SDMMC_GPIO_VALUE(x, y) (GPIO_CLK_DIV(x) | GPIO_USE_SAMPLE_DLY(y))

#define SDMMC_UHS_REG_EXT	0x108
#define SDMMC_ENABLE_SHIFT	0x110

#define TIMING_MODE     3
#define TIMING_CFG_NUM 10

#define PULL_DOWN BIT(1)
#define PULL_UP   BIT(0)

#define NUM_PHASES (40)

#define ENABLE_SHIFT_MIN_SMPL (4)
#define ENABLE_SHIFT_MAX_SMPL (12)
#define USE_DLY_MIN_SMPL (11)
#define USE_DLY_MAX_SMPL (14)

struct k3_priv {
	u8 ctrl_id;
	u32 cur_speed;
	struct regmap	*reg;
};

static unsigned long dw_mci_hi6220_caps[] = {
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	0
};

struct hs_timing {
	int drv_phase;
	int sam_dly;
	int sam_phase_max;
	int sam_phase_min;
};

struct hs_timing hs_timing_cfg[TIMING_MODE][TIMING_CFG_NUM] = {
	{ /* reserved */ },
	{ /* SD */
		{7, 0, 15, 15,},  /* 0: LEGACY 400k */
		{6, 0,  4,  4,},  /* 1: MMC_HS */
		{6, 0,  3,  3,},  /* 2: SD_HS */
		{6, 0, 15, 15,},  /* 3: SDR12 */
		{6, 0,  2,  2,},  /* 4: SDR25 */
		{4, 0, 11,  0,},  /* 5: SDR50 */
		{6, 4, 15,  0,},  /* 6: SDR104 */
		{0},              /* 7: DDR50 */
		{0},              /* 8: DDR52 */
		{0},              /* 9: HS200 */
	},
	{ /* SDIO */
		{7, 0, 15, 15,},  /* 0: LEGACY 400k */
		{0},              /* 1: MMC_HS */
		{6, 0, 15, 15,},  /* 2: SD_HS */
		{6, 0, 15, 15,},  /* 3: SDR12 */
		{6, 0,  0,  0,},  /* 4: SDR25 */
		{4, 0, 12,  0,},  /* 5: SDR50 */
		{5, 4, 15,  0,},  /* 6: SDR104 */
		{0},              /* 7: DDR50 */
		{0},              /* 8: DDR52 */
		{0},              /* 9: HS200 */
	}
};

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

	priv->ctrl_id = of_alias_get_id(host->dev->of_node, "mshc");
	if (priv->ctrl_id < 0)
		priv->ctrl_id = 0;

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

static int dw_mci_hi6220_execute_tuning(struct dw_mci_slot *slot, u32 opcode)
{
	return 0;
}

static const struct dw_mci_drv_data hi6220_data = {
	.caps			= dw_mci_hi6220_caps,
	.switch_voltage		= dw_mci_hi6220_switch_voltage,
	.set_ios		= dw_mci_hi6220_set_ios,
	.parse_dt		= dw_mci_hi6220_parse_dt,
	.execute_tuning		= dw_mci_hi6220_execute_tuning,
};

static void dw_mci_hs_set_timing(struct dw_mci *host, int timing, int sam_phase)
{
	int drv_phase;
	int sam_dly;
	int ctrl_id;
	int use_sam_dly = 0;
	int enable_shift = 0;
	int reg_value;
	struct k3_priv *priv;

	priv = host->priv;
	ctrl_id = priv->ctrl_id;

	drv_phase = hs_timing_cfg[ctrl_id][timing].drv_phase;
	sam_dly   = hs_timing_cfg[ctrl_id][timing].sam_dly;
	if (sam_phase == -1)
		sam_phase = (hs_timing_cfg[ctrl_id][timing].sam_phase_max +
			     hs_timing_cfg[ctrl_id][timing].sam_phase_min) / 2;

	if (timing == MMC_TIMING_UHS_SDR50 ||
	    timing == MMC_TIMING_UHS_SDR104) {
		if (sam_phase >= ENABLE_SHIFT_MIN_SMPL &&
		    sam_phase <= ENABLE_SHIFT_MAX_SMPL)
			enable_shift = 1;
	}
	if (timing == MMC_TIMING_UHS_SDR104) {
		if (sam_phase >= USE_DLY_MIN_SMPL &&
		    sam_phase <= USE_DLY_MAX_SMPL)
			use_sam_dly = 1;
	}

	mci_writel(host, GPIO, 0x0);
	udelay(5);

	reg_value = SDMMC_UHS_REG_EXT_VALUE(sam_phase, sam_dly, drv_phase);
	mci_writel(host, UHS_REG_EXT, reg_value);

	mci_writel(host, ENABLE_SHIFT, enable_shift);

	reg_value = SDMMC_GPIO_VALUE(GENCLK_DIV, use_sam_dly);
	mci_writel(host, GPIO, (unsigned int)reg_value | GPIO_CLK_ENABLE);
}

int dw_mci_hi3660_init(struct dw_mci *host)
{
	/* set threshold to 512 bytes */
	mci_writel(host, CDTHRCTL, 0x02000001);

	dw_mci_hs_set_timing(host, MMC_TIMING_LEGACY, -1);
	host->bus_hz /= (GENCLK_DIV + 1);

	return 0;
}

static int dw_mci_set_sel18(struct dw_mci *host, bool set)
{
	int ret;
	unsigned int val;
	struct k3_priv *priv;

	priv = host->priv;

	val = set ? SDCARD_IO_SEL18 : 0;
	ret = regmap_update_bits(priv->reg, SOC_SCTRL_SCPERCTRL5,
				 SDCARD_IO_SEL18, val);
	if (ret) {
		dev_err(host->dev, "sel18 %u error\n", val);
		return ret;
	}

	return 0;
}

void dw_mci_hi3660_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	int ret;
	unsigned long wanted;
	unsigned long actual;
	struct k3_priv *priv = host->priv;

	if (!ios->clock || ios->clock == priv->cur_speed)
		return;

	wanted = ios->clock * (GENCLK_DIV + 1);
	ret = clk_set_rate(host->ciu_clk, wanted);
	if (ret) {
		dev_err(host->dev, "failed to set rate %luHz\n", wanted);
		return;
	}
	actual = clk_get_rate(host->ciu_clk);

	dw_mci_hs_set_timing(host, ios->timing, -1);
	host->bus_hz = actual / (GENCLK_DIV + 1);
	host->current_speed = 0;
	priv->cur_speed = host->bus_hz;
}

static int dw_mci_get_best_clksmpl(unsigned int sample_flag)
{
	int i;
	int interval;
	unsigned int v;
	unsigned int len;
	unsigned int range_start = 0;
	unsigned int range_length = 0;
	unsigned int middle_range = 0;

	if (!sample_flag)
		return -EIO;

	if (~sample_flag == 0)
		return 0;

	i = ffs(sample_flag) - 1;

	while (i < 32) {
		v = ror32(sample_flag, i);
		len = ffs(~v) - 1;

		if (len > range_length) {
			range_length = len;
			range_start = i;
		}

		interval = ffs(v >> len) - 1;
		if (interval < 0)
			break;

		i += len + interval;
	}

	middle_range = range_start + range_length / 2;
	if (middle_range >= 32)
		middle_range %= 32;

	return middle_range;
}

static int dw_mci_hi3660_execute_tuning(struct dw_mci_slot *slot, u32 opcode)
{
	int i = 0;
	struct dw_mci *host = slot->host;
	struct mmc_host *mmc = slot->mmc;
	int sam_phase = 0;
	u32 tuning_sample_flag = 0;
	int best_clksmpl = 0;

	for (i = 0; i < NUM_PHASES; ++i, ++sam_phase) {
		sam_phase %= 32;

		mci_writel(host, TMOUT, ~0);
		dw_mci_hs_set_timing(host, mmc->ios.timing, sam_phase);

		if (!mmc_send_tuning(mmc, opcode, NULL))
			tuning_sample_flag |= (1 << sam_phase);
		else
			tuning_sample_flag &= ~(1 << sam_phase);
	}

	best_clksmpl = dw_mci_get_best_clksmpl(tuning_sample_flag);
	if (best_clksmpl < 0) {
		dev_err(host->dev, "All phases bad!\n");
		return -EIO;
	}

	dw_mci_hs_set_timing(host, mmc->ios.timing, best_clksmpl);

	dev_info(host->dev, "tuning ok best_clksmpl %u tuning_sample_flag %x\n",
		 best_clksmpl, tuning_sample_flag);
	return 0;
}

static int dw_mci_hi3660_switch_voltage(struct mmc_host *mmc,
					struct mmc_ios *ios)
{
	int ret;
	int min_uv = 0;
	int max_uv = 0;
	struct dw_mci_slot *slot = mmc_priv(mmc);
	struct k3_priv *priv;
	struct dw_mci *host;

	host = slot->host;
	priv = host->priv;

	if (!priv || !priv->reg)
		return 0;

	if (priv->ctrl_id == DWMMC_SDIO_ID)
		return 0;

	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		ret = dw_mci_set_sel18(host, 0);
		if (ret)
			return ret;
		min_uv = 2950000;
		max_uv = 2950000;
	} else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
		ret = dw_mci_set_sel18(host, 1);
		if (ret)
			return ret;
		min_uv = 1800000;
		max_uv = 1800000;
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

static const struct dw_mci_drv_data hi3660_data = {
	.init = dw_mci_hi3660_init,
	.set_ios = dw_mci_hi3660_set_ios,
	.parse_dt = dw_mci_hi6220_parse_dt,
	.execute_tuning = dw_mci_hi3660_execute_tuning,
	.switch_voltage  = dw_mci_hi3660_switch_voltage,
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

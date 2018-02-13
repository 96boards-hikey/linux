/**
 * dwc3-hisi.c - Support for dwc3 platform devices on HiSilicon platforms
 *
 * Copyright (C) 2017-2018 Hilisicon Electronics Co., Ltd.
 *		http://www.huawei.com
 *
 * Authors: Yu Chen <chenyu56@huawei.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/extcon.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

struct dwc3_hisi {
	struct device		*dev;
	struct regulator	*usb_regu;
	struct clk		**clks;
	int			num_clocks;
	struct reset_control	**rstcs;
	int			num_rstcs;
};

struct dwc3_hisi *g_dwc3_hisi;

static int dwc3_hisi_init_clks(struct dwc3_hisi *dwc3_hisi,
		struct device_node *np)
{
	struct device *dev = dwc3_hisi->dev;
	int i;

	dwc3_hisi->num_clocks = of_clk_get_parent_count(np);
	if (!dwc3_hisi->num_clocks)
		return -ENOENT;

	dwc3_hisi->clks = devm_kcalloc(dev, dwc3_hisi->num_clocks,
			sizeof(struct clk *), GFP_KERNEL);
	if (!dwc3_hisi->clks)
		return -ENOMEM;

	for (i = 0; i < dwc3_hisi->num_clocks; i++) {
		struct clk	*clk;

		clk = of_clk_get(np, i);
		if (IS_ERR(clk)) {
			while (--i >= 0)
				clk_put(dwc3_hisi->clks[i]);

			return PTR_ERR(clk);
		}

		dwc3_hisi->clks[i] = clk;
	}

	return 0;
}

static int dwc3_hisi_enable_clks(struct dwc3_hisi *dwc3_hisi)
{
	int i;
	int ret;

	for (i = 0; i < dwc3_hisi->num_clocks; i++) {
		ret = clk_prepare_enable(dwc3_hisi->clks[i]);
		if (ret < 0) {
			while (--i >= 0)
				clk_disable_unprepare(dwc3_hisi->clks[i]);

			return ret;
		}
	}

	return 0;
}

static void dwc3_hisi_disable_clks(struct dwc3_hisi *dwc3_hisi)
{
	int i;

	for (i = 0; i < dwc3_hisi->num_clocks; i++)
		clk_disable_unprepare(dwc3_hisi->clks[i]);
}

static int dwc3_hisi_get_rstcs(struct dwc3_hisi *dwc3_hisi,
		struct device_node *np)
{
	struct device *dev = dwc3_hisi->dev;
	int i;

	dwc3_hisi->num_rstcs = of_count_phandle_with_args(np,
			"resets", "#reset-cells");
	if (!dwc3_hisi->num_rstcs)
		return -ENOENT;

	dwc3_hisi->rstcs = devm_kcalloc(dev, dwc3_hisi->num_rstcs,
			sizeof(struct reset_control *), GFP_KERNEL);
	if (!dwc3_hisi->rstcs)
		return -ENOMEM;

	for (i = 0; i < dwc3_hisi->num_rstcs; i++) {
		struct reset_control *rstc;

		rstc = of_reset_control_get_shared_by_index(np, i);
		if (IS_ERR(rstc)) {
			while (--i >= 0)
				reset_control_put(dwc3_hisi->rstcs[i]);
			return PTR_ERR(rstc);
		}

		dwc3_hisi->rstcs[i] = rstc;
	}

	return 0;
}

static int dwc3_hisi_reset_control_assert(struct dwc3_hisi *dwc3_hisi)
{
	int i, ret;

	for (i = dwc3_hisi->num_rstcs - 1; i >= 0 ; i--) {
		ret = reset_control_assert(dwc3_hisi->rstcs[i]);
		if (ret) {
			while (--i >= 0)
				reset_control_deassert(dwc3_hisi->rstcs[i]);
			return ret;
		}
		udelay(10);
	}

	return 0;
}

static int dwc3_hisi_reset_control_deassert(struct dwc3_hisi *dwc3_hisi)
{
	int i, ret;

	for (i = 0; i < dwc3_hisi->num_rstcs; i++) {
		ret = reset_control_deassert(dwc3_hisi->rstcs[i]);
		if (ret) {
			while (--i >= 0)
				reset_control_assert(dwc3_hisi->rstcs[i]);
			return ret;
		}
		udelay(10);
	}

	return 0;
}

static int dwc3_hisi_probe(struct platform_device *pdev)
{
	struct dwc3_hisi	*dwc3_hisi;
	struct device		*dev = &pdev->dev;
	struct device_node	*np = dev->of_node;

	int			ret;
	int			i;

	dwc3_hisi = devm_kzalloc(dev, sizeof(*dwc3_hisi), GFP_KERNEL);
	if (!dwc3_hisi)
		return -ENOMEM;

	platform_set_drvdata(pdev, dwc3_hisi);
	dwc3_hisi->dev = dev;

	ret = dwc3_hisi_init_clks(dwc3_hisi, np);
	if (ret) {
		dev_err(dev, "could not get clocks\n");
		return ret;
	}

	ret = dwc3_hisi_enable_clks(dwc3_hisi);
	if (ret) {
		dev_err(dev, "could not enable clocks\n");
		goto err_put_clks;
	}

	ret = dwc3_hisi_get_rstcs(dwc3_hisi, np);
	if (ret) {
		dev_err(dev, "could not get reset controllers\n");
		goto err_disable_clks;
	}
	ret = dwc3_hisi_reset_control_deassert(dwc3_hisi);
	if (ret) {
		dev_err(dev, "reset control deassert failed\n");
		goto err_put_rstcs;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "failed to add dwc3 core\n");
		goto err_reset_assert;
	}

	g_dwc3_hisi = dwc3_hisi;
	return 0;

err_reset_assert:
	ret = dwc3_hisi_reset_control_assert(dwc3_hisi);
	if (ret)
		dev_err(dev, "reset control assert failed\n");
err_put_rstcs:
	for (i = 0; i < dwc3_hisi->num_rstcs; i++)
		reset_control_put(dwc3_hisi->rstcs[i]);
err_disable_clks:
	dwc3_hisi_disable_clks(dwc3_hisi);
err_put_clks:
	for (i = 0; i < dwc3_hisi->num_clocks; i++)
		clk_put(dwc3_hisi->clks[i]);

	return ret;
}

static int dwc3_hisi_remove(struct platform_device *pdev)
{
	struct dwc3_hisi	*dwc3_hisi = platform_get_drvdata(pdev);
	struct device		*dev = &pdev->dev;
	int			i, ret;

	of_platform_depopulate(dev);

	ret = dwc3_hisi_reset_control_assert(dwc3_hisi);
	if (ret) {
		dev_err(dev, "reset control assert failed\n");
		return ret;
	}

	for (i = 0; i < dwc3_hisi->num_clocks; i++) {
		clk_disable_unprepare(dwc3_hisi->clks[i]);
		clk_put(dwc3_hisi->clks[i]);
	}

	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_hisi_suspend(struct device *dev)
{
	struct dwc3_hisi *dwc3_hisi = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "%s\n", __func__);

	ret = dwc3_hisi_reset_control_assert(dwc3_hisi);
	if (ret) {
		dev_err(dev, "reset control assert failed\n");
		return ret;
	}

	dwc3_hisi_disable_clks(dwc3_hisi);
	pinctrl_pm_select_default_state(dev);
	return 0;
}

static int dwc3_hisi_resume(struct device *dev)
{
	struct dwc3_hisi *dwc3_hisi = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "%s\n", __func__);
	pinctrl_pm_select_default_state(dev);

	ret = dwc3_hisi_enable_clks(dwc3_hisi);
	if (ret) {
		dev_err(dev, "could not enable clocks\n");
		return ret;
	}

	ret = dwc3_hisi_reset_control_deassert(dwc3_hisi);
	if (ret) {
		dev_err(dev, "reset control deassert failed\n");
		return ret;
	}

	/* Wait for clock stable */
	msleep(100);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(dwc3_hisi_dev_pm_ops,
		dwc3_hisi_suspend, dwc3_hisi_resume);

static const struct of_device_id dwc3_hisi_match[] = {
	{ .compatible = "hisilicon,hi3660-dwc3" },
	{ .compatible = "hisilicon,kirin970-dwc3" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, dwc3_hisi_match);

static struct platform_driver dwc3_hisi_driver = {
	.probe = dwc3_hisi_probe,
	.remove = dwc3_hisi_remove,
	.driver = {
		.name = "usb-hisi-dwc3",
		.of_match_table = dwc3_hisi_match,
		.pm = &dwc3_hisi_dev_pm_ops,
	},
};

module_platform_driver(dwc3_hisi_driver);

MODULE_AUTHOR("Yu Chen <chenyu56@huawei.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 HiSilicon Glue Layer");

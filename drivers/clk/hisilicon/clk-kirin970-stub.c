// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/clk/hisilicon/clk-kirin970-stub.c
 *
 * Copyright (c) 2018 Hisilicon Limited.
 *
 * Author: Kevin Wangtao <kevin.wangtao@hisilicon.com>
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <dt-bindings/clock/kirin970-clock.h>

#define MHZ	1000000

struct hisi_stub_clk {
	unsigned int id;
	struct device *dev;
	struct clk_hw hw;
	const char *clk_name;
	unsigned long rate;
	unsigned int convert_ratio;
	unsigned int clk_get_offset;
	unsigned int clk_set_offset;
	unsigned int clk_get_mask;
	unsigned int clk_set_mask;
};

struct hisi_stub_clk_data {
	struct hisi_stub_clk *clk_table;
	unsigned int clk_num;
	void __iomem *freq_get_base;
	void __iomem *freq_set_base;
};

static struct hisi_stub_clk kirin970_stub_clk[KIRIN970_CLK_STUB_NUM] = {
	[KIRIN970_CLK_STUB_CLUSTER0] = {
		.id = KIRIN970_CLK_STUB_CLUSTER0,
		.clk_name = "cpu-cluster.0",
		.convert_ratio = 16,
		.clk_get_offset = 0x70,
		.clk_set_offset = 0x280,
		.clk_get_mask = 0xFFFF,
		.clk_set_mask = 0xFF,
	},
	[KIRIN970_CLK_STUB_CLUSTER1] = {
		.id = KIRIN970_CLK_STUB_CLUSTER1,
		.clk_name = "cpu-cluster.1",
		.convert_ratio = 16,
		.clk_get_offset = 0x74,
		.clk_set_offset = 0x270,
		.clk_get_mask = 0xFFFF,
		.clk_set_mask = 0xFF,
	},
	[KIRIN970_CLK_STUB_GPU] = {
		.id = KIRIN970_CLK_STUB_GPU,
		.clk_name = "clk-g3d",
		.convert_ratio = 16,
		.clk_get_offset = 0x78,
		.clk_set_offset = 0x290,
		.clk_get_mask = 0xFFFF,
		.clk_set_mask = 0xFF,
	},
	[KIRIN970_CLK_STUB_DDR] = {
		.id = KIRIN970_CLK_STUB_DDR,
		.clk_name = "clk-ddrc",
		.clk_get_offset = 0x7C,
		.clk_get_mask = 0xFFFF,
	},
	[KIRIN970_CLK_STUB_DDR_VOTE] = {
		.id = KIRIN970_CLK_STUB_DDR_VOTE,
		.clk_name = "clk-ddrc-vote",
		.convert_ratio = 16,
		.clk_set_offset = 0x2B0,
		.clk_set_mask = 0xFF,
	},
	[KIRIN970_CLK_STUB_DDR_LIMIT] = {
		.id = KIRIN970_CLK_STUB_DDR_LIMIT,
		.clk_name = "clk-ddrc-limit",
		.convert_ratio = 16,
		.clk_set_offset = 0x2A0,
		.clk_set_mask = 0xFF,
	},
};

static struct hisi_stub_clk_data kirin970_stub_clk_data = {
	.clk_table = kirin970_stub_clk,
	.clk_num = KIRIN970_CLK_STUB_NUM,
};


static unsigned long hisi_stub_clk_recalc_rate(
		struct clk_hw *hw, unsigned long parent_rate)
{
	struct hisi_stub_clk *stub_clk =
		container_of(hw, struct hisi_stub_clk, hw);
	struct platform_device *pdev = container_of(stub_clk->dev,
					struct platform_device, dev);
	struct hisi_stub_clk_data *plat_data = platform_get_drvdata(pdev);
	unsigned long rate;
	int shift;

	if (stub_clk->clk_get_mask) {
		shift = ffs(stub_clk->clk_get_mask) - 1;
		rate = readl(plat_data->freq_get_base +
			     stub_clk->clk_get_offset);
		rate &= stub_clk->clk_get_mask;
		stub_clk->rate = (rate >> shift) * MHZ;
	}

	pr_debug("get rate%d;%ld\n", stub_clk->id, stub_clk->rate);

	return stub_clk->rate;
}

static long hisi_stub_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				       unsigned long *prate)
{
	return rate;
}

int hisi_stub_clk_determine_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	pr_debug("%s: enter %ld\n", __func__, req->rate);
	return 0;
}

static int hisi_stub_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct hisi_stub_clk *stub_clk =
		container_of(hw, struct hisi_stub_clk, hw);
	struct platform_device *pdev = container_of(stub_clk->dev,
					struct platform_device, dev);
	struct hisi_stub_clk_data *plat_data = platform_get_drvdata(pdev);
	unsigned int freq_cfg, mask;
	int shift;

	if (stub_clk->clk_set_mask) {
		freq_cfg = rate / MHZ;
		if (stub_clk->convert_ratio)
			freq_cfg /= stub_clk->convert_ratio;

		shift = ffs(stub_clk->clk_set_mask) - 1;
		mask  = stub_clk->clk_set_mask >> shift;

		if (freq_cfg > mask)
			freq_cfg = mask;

		freq_cfg <<= shift;
		freq_cfg |= stub_clk->clk_set_mask << 16;
		writel(freq_cfg,
		       stub_clk->clk_set_offset + plat_data->freq_set_base);
	}

	pr_debug("set rate%d;%ld\n", stub_clk->id, rate);

	stub_clk->rate = rate;

	return 0;
}

static struct clk_ops hisi_stub_clk_ops = {
	.recalc_rate    = hisi_stub_clk_recalc_rate,
	.determine_rate = hisi_stub_clk_determine_rate,
	.round_rate     = hisi_stub_clk_round_rate,
	.set_rate       = hisi_stub_clk_set_rate,
};

static struct clk *hisi_register_stub_clk(struct device *dev,
		struct hisi_stub_clk *stub_clk)
{
	struct clk_init_data init = {};
	struct clk *clk;

	stub_clk->hw.init = &init;
	stub_clk->dev = dev;

	init.name = stub_clk->clk_name;
	init.ops = &hisi_stub_clk_ops;
	init.num_parents = 0;
	init.flags = CLK_GET_RATE_NOCACHE;

	clk = devm_clk_register(dev, &stub_clk->hw);
	if (!IS_ERR(clk))
		dev_dbg(dev, "Registered clock '%s'\n", init.name);

	return clk;
}

static const struct of_device_id hisi_stub_clk_of_match[] = {
	{
		.compatible = "hisilicon,kirin970-stub-clk",
		.data = &kirin970_stub_clk_data,
	},
	{}
};

static int hisi_stub_clk_probe(struct platform_device *pdev)
{
	struct hisi_stub_clk_data *plat_data;
	const struct of_device_id *match;
	struct clk_onecell_data	*data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct clk **clk_table;
	struct clk *clk;
	unsigned int idx;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "could not allocate clock data\n");
		return -ENOMEM;
	}

	match = of_match_device(hisi_stub_clk_of_match, dev);
	plat_data = (struct hisi_stub_clk_data *) match->data;
	if (!plat_data) {
		dev_err(dev, "no clock data\n");
		return -EINVAL;
	}
	data->clk_num = plat_data->clk_num;

	clk_table = devm_kzalloc(dev,
		sizeof(*clk_table) * plat_data->clk_num, GFP_KERNEL);
	if (IS_ERR_OR_NULL(clk_table)) {
		dev_err(dev, "could not allocate clock lookup table\n");
		return -ENOMEM;
	}
	data->clks = clk_table;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	plat_data->freq_get_base = devm_ioremap(dev,
				res->start, resource_size(res));
	if (IS_ERR(plat_data->freq_get_base)) {
		dev_err(dev, "failed get freq get memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	plat_data->freq_set_base = devm_ioremap(dev,
				res->start, resource_size(res));
	if (IS_ERR(plat_data->freq_set_base)) {
		dev_err(dev, "failed get freq set memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, plat_data);

	for (idx = 0; idx < plat_data->clk_num; idx++) {
		clk = hisi_register_stub_clk(dev, &plat_data->clk_table[idx]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);

		data->clks[idx] = clk;
	}
	of_clk_add_provider(pdev->dev.of_node, of_clk_src_onecell_get, data);

	return 0;
}

static struct platform_driver hisi_stub_clk_driver = {
	.driver = {
		.name = "kirin970-stub-clk",
		.of_match_table = hisi_stub_clk_of_match,
	},
	.probe = hisi_stub_clk_probe,
};

static int __init hisi_stub_clk_init(void)
{
	return platform_driver_register(&hisi_stub_clk_driver);
}
subsys_initcall(hisi_stub_clk_init);

/*
 * Hisilicon clock driver
 *
 * Copyright (c) 2013-2015 Hisilicon Limited.
 * Copyright (c) 2017 Linaro Limited.
 * Copyright (c) 2017 Hisilicon Limited.
 *
 * Author: Kai Zhao <zhaokai1@hisilicon.com>
 * Author: Leo Yan <leo.yan@linaro.org>
 * Author: Tao Wang <kevin.wangtao@hisilicon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include <linux/sched.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/mailbox_client.h>
#include <dt-bindings/clock/hi3660-clock.h>

#define FREQ_DATA_OFFSET	0x70
#define MHZ			1000000

struct hi3660_stub_clk_chan {
	struct mbox_client cl;
	struct mbox_chan *mbox;
};

struct hi3660_stub_clk {
	unsigned int id;
	struct device *dev;
	struct clk_hw hw;
	const char *clk_name;
	unsigned int set_rate_cmd;
	unsigned int msg[8];
	unsigned int rate;
};

static void __iomem *freq_reg;
static struct hi3660_stub_clk_chan *chan;

static struct hi3660_stub_clk hisi_stub_clk[HI3660_CLK_STUB_NUM] = {
	[HI3660_CLK_STUB_CLUSTER0] = {
		.id = HI3660_CLK_STUB_CLUSTER0,
		.clk_name = "cpu-cluster.0",
		.set_rate_cmd = 0x0001030A,
	},
	[HI3660_CLK_STUB_CLUSTER1] = {
		.id = HI3660_CLK_STUB_CLUSTER1,
		.clk_name = "cpu-cluster.1",
		.set_rate_cmd = 0x0002030A,
	},
	[HI3660_CLK_STUB_GPU] = {
		.id = HI3660_CLK_STUB_GPU,
		.clk_name = "clk-g3d",
		.set_rate_cmd = 0x0003030A,
	},
	[HI3660_CLK_STUB_DDR] = {
		.id = HI3660_CLK_STUB_DDR,
		.clk_name = "clk-ddrc",
		.set_rate_cmd = 0x00040309,
	},
};

static unsigned long hi3660_stub_clk_recalc_rate(
		struct clk_hw *hw, unsigned long parent_rate)
{
	struct hi3660_stub_clk *stub_clk =
		container_of(hw, struct hi3660_stub_clk, hw);

	if (stub_clk->id < HI3660_CLK_STUB_NUM)
		stub_clk->rate = readl(freq_reg + (stub_clk->id << 2)) * MHZ;

	pr_debug("get rate%d;%u\n", stub_clk->id, stub_clk->rate);

	return stub_clk->rate;
}

static long hi3660_stub_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				       unsigned long *prate)
{
	return rate;
}

int hi3660_stub_clk_determine_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	pr_debug("%s: enter %ld\n", __func__, req->rate);
	return 0;
}

static int hi3660_stub_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct hi3660_stub_clk *stub_clk =
		container_of(hw, struct hi3660_stub_clk, hw);

	if (stub_clk->id < HI3660_CLK_STUB_NUM) {
		stub_clk->msg[0] = stub_clk->set_rate_cmd;
		stub_clk->msg[1] = rate / MHZ;

		pr_debug("%s: set_rate_cmd[0] %x [1] %x\n", __func__,
				stub_clk->msg[0], stub_clk->msg[1]);

		mbox_send_message(chan->mbox, stub_clk->msg);
	}

	stub_clk->rate = rate;
	return 0;
}

static struct clk_ops hi3660_stub_clk_ops = {
	.recalc_rate    = hi3660_stub_clk_recalc_rate,
	.determine_rate = hi3660_stub_clk_determine_rate,
	.round_rate     = hi3660_stub_clk_round_rate,
	.set_rate       = hi3660_stub_clk_set_rate,
};

static struct clk *hi3660_register_stub_clk(struct device *dev,
		struct hi3660_stub_clk *stub_clk)
{
	struct clk_init_data init = {};
	struct clk *clk;

	stub_clk->hw.init = &init;
	stub_clk->dev = dev;

	init.name = stub_clk->clk_name;
	init.ops = &hi3660_stub_clk_ops;
	init.num_parents = 0;
	init.flags = CLK_GET_RATE_NOCACHE;

	clk = devm_clk_register(dev, &stub_clk->hw);
	if (!IS_ERR(clk))
		dev_dbg(dev, "Registered clock '%s'\n", init.name);

	return clk;
}

static int hi3660_stub_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct clk_onecell_data	*data;
	struct resource *res;
	struct clk **clk_table;
	struct clk *clk;
	unsigned int idx;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "could not allocate clock data\n");
		return -ENOMEM;
	}

	clk_table = devm_kzalloc(dev,
		sizeof(*clk_table) * HI3660_CLK_STUB_NUM, GFP_KERNEL);
	if (!clk_table) {
		dev_err(dev, "could not allocate clock lookup table\n");
		return -ENOMEM;
	}
	data->clks = clk_table;
	data->clk_num = HI3660_CLK_STUB_NUM;

	chan = devm_kzalloc(dev, sizeof(*chan), GFP_KERNEL);
	if (!chan) {
		dev_err(dev, "failed to allocate memory for mbox\n");
		return -ENOMEM;
	}

	/* Use mailbox client with blocking mode */
	chan->cl.dev = dev;
	chan->cl.tx_done = NULL;
	chan->cl.tx_block = false;
	chan->cl.tx_tout = 500;
	chan->cl.knows_txdone = false;

	/* Allocate mailbox channel */
	chan->mbox = mbox_request_channel(&chan->cl, 0);
	if (IS_ERR(chan->mbox)) {
		dev_err(dev, "failed get mailbox channel\n");
		return PTR_ERR(chan->mbox);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	freq_reg = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(freq_reg)) {
		dev_err(dev, "failed get shared memory\n");
		return -ENOMEM;
	}
	freq_reg += FREQ_DATA_OFFSET;

	for (idx = 0; idx < HI3660_CLK_STUB_NUM; idx++) {
		clk = hi3660_register_stub_clk(dev, &hisi_stub_clk[idx]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);

		data->clks[idx] = clk;
	}
	of_clk_add_provider(np, of_clk_src_onecell_get, data);

	return 0;
}

static const struct of_device_id hi3660_stub_clk_of_match[] = {
	{ .compatible = "hisilicon,hi3660-stub-clk", },
	{}
};

static struct platform_driver hi3660_stub_clk_driver = {
	.driver = {
		.name = "hi3660-stub-clk",
		.of_match_table = hi3660_stub_clk_of_match,
	},
	.probe = hi3660_stub_clk_probe,
};

static int __init hi3660_stub_clk_init(void)
{
	return platform_driver_register(&hi3660_stub_clk_driver);
}
subsys_initcall(hi3660_stub_clk_init);

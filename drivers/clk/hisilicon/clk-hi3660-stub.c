/*
 * Hisilicon clock driver
 *
 * Copyright (c) 2013-2015 Hisilicon Limited.
 * Copyright (c) 2017 Linaro Limited.
 *
 * Author: Kai Zhao <zhaokai1@hisilicon.com>
 * Author: Leo Yan <leo.yan@linaro.org>
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
#include <linux/clk/clk-conf.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/clkdev.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/mailbox_client.h>
#include <dt-bindings/clock/hi3660-clock.h>

#include "clk.h"

struct hi3660_stub_clk_chan {
	struct mbox_client cl;
	struct mbox_chan *mbox;
};

struct hi3660_stub_clk {

	u32 id;
	struct device *dev;
	struct clk_hw hw;

	u32 set_rate_cmd;
	u32 get_rate_cmd;

	u32 *table;
	u32 table_len;

	u32 rate;
	unsigned int msg[8];

	struct hi3660_stub_clk_chan *chan;
};

/*
 * Below code is reserved to enable DDR frequency change,
 * looks the below commented code is not necessary and it's
 * not clean for upstreaming to mainline kernel.
 */
#if 0

#define FREQ_INDEX_MASK	   0xF
static unsigned int hi3660_stub_clk_get_freq_idx(struct hi3660_stub_clk *stub_clk)
{
	unsigned int sys_bak_reg;

	if (!stub_clk->reg)
		return 0;

	sys_bak_reg = readl(stub_clk->reg);

	/*sysctrl SCBAKDATA4
		bit 0-3	      LITTLE Cluster
		bit 4-7	      BIG Cluster
		bit 8-11    DDR
		bit 12-15  GPU*/
	switch (stub_clk->id) {
	case 0:
		break;
	case 1:
		sys_bak_reg >>= 4;
		break;
	case 2:
		sys_bak_reg >>= 12;
		break;
	case 3:
		sys_bak_reg >>= 8;
		break;
	default:
		return 0;
	}

	sys_bak_reg &= FREQ_INDEX_MASK;

	if (sys_bak_reg >= stub_clk->table_length)
		sys_bak_reg = 0;

	return sys_bak_reg;
}
#endif

static unsigned long hi3660_stub_clk_recalc_rate(struct clk_hw *hw,
						 unsigned long parent_rate)
{
	struct hi3660_stub_clk *stub_clk = container_of(hw, struct hi3660_stub_clk, hw);
	u32 rate;

#if 0
	printk("%s: enter\n", __func__);

	switch (stub_clk->id) {
	/* DDR get freq */
	case 3:
		freq_index = hi3660_stub_clk_get_freq_idx(stub_clk);
		rate = stub_clk->freq[freq_index]*1000;
		pr_debug("[%s]3 idx=%d rate=%d\n", __func__, freq_index, rate);
		break;
	/* DDR set min */
	case 4:
	default:
		rate = stub_clk->rate;
	}
#endif
	if (!stub_clk->rate)
		stub_clk->rate = stub_clk->table[0];

	rate = stub_clk->rate;
	return rate;
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

	stub_clk->msg[0] = stub_clk->set_rate_cmd;
	stub_clk->msg[1] = rate / 1000000;

	pr_debug("%s: set_rate_cmd[0] %x [1] %x\n", __func__,
		stub_clk->msg[0], stub_clk->msg[1]);

	mbox_send_message(stub_clk->chan->mbox, stub_clk->msg);
	stub_clk->rate = rate;
	return 0;
}

static struct clk_ops hi3660_stub_clk_ops = {
	.recalc_rate    = hi3660_stub_clk_recalc_rate,
	.determine_rate = hi3660_stub_clk_determine_rate,
	.round_rate     = hi3660_stub_clk_round_rate,
	.set_rate       = hi3660_stub_clk_set_rate,
};

static int hi3660_register_stub_clk(struct platform_device *pdev,
		int id, char *clk_name,
		u32 set_rate_cmd, u32 get_rate_cmd,
		struct hi3660_stub_clk_chan *chan,
		u32 *table, u32 table_len,
		struct clk_onecell_data *data)
{
	struct device *dev = &pdev->dev;
	struct clk_init_data init;
	struct hi3660_stub_clk *stub_clk;
	struct clk *clk;

	stub_clk = devm_kzalloc(dev, sizeof(*stub_clk), GFP_KERNEL);
	if (!stub_clk)
		return -ENOMEM;

	stub_clk->hw.init = &init;
	stub_clk->dev = dev;
	stub_clk->id = id;
	stub_clk->set_rate_cmd = set_rate_cmd;
	stub_clk->get_rate_cmd = get_rate_cmd;
	stub_clk->chan = chan;
	stub_clk->table = table;
	stub_clk->table_len = table_len;

	init.name = kstrdup(clk_name, GFP_KERNEL);
	init.ops = &hi3660_stub_clk_ops;
	init.num_parents = 0;
	init.flags = 0;

	clk = devm_clk_register(dev, &stub_clk->hw);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	data->clks[id] = clk;
	dev_dbg(dev, "Registered clock '%s'\n", init.name);
	return 0;
}

static u32 ca53_freq[] = {
	533000, 999000, 1402000, 1709000, 1844000,
};

static u32 ca72_freq[] = {
	903000, 1421000, 1805000, 2112000, 2362000, 2612000,
};

static u32 ddr_freq[] = {
	400000, 685000, 1067000, 1245000, 1600000, 1866000,
};

static u32 gpu_freq[] = {
	400000, 533000, 807000, 884000,
};

static int hi3660_stub_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hi3660_stub_clk_chan *chan;
	struct device_node *np = pdev->dev.of_node;
	struct clk_onecell_data	*data;
	struct clk **clk_table;

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
	of_clk_add_provider(np, of_clk_src_onecell_get, data);

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

	hi3660_register_stub_clk(pdev, HI3660_CLK_STUB_CLUSTER0,
			"cpu-cluster.0", 0x0001030a, 0x0001020a, chan,
			ca53_freq, ARRAY_SIZE(ca53_freq), data);
	hi3660_register_stub_clk(pdev, HI3660_CLK_STUB_CLUSTER1,
			"cpu-cluster.1", 0x0002030a, 0x0002020a, chan,
			ca72_freq, ARRAY_SIZE(ca72_freq), data);
	hi3660_register_stub_clk(pdev, HI3660_CLK_STUB_GPU,
			"clk-g3d", 0x0003030a, 0x0003020a, chan,
			gpu_freq, ARRAY_SIZE(gpu_freq), data);
	hi3660_register_stub_clk(pdev, HI3660_CLK_STUB_DDR,
			"clk-ddrc", 0x0004030a, 0x0004020a, chan,
			ddr_freq, ARRAY_SIZE(ddr_freq), data);

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

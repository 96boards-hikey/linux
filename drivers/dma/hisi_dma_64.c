/*
 * Copyright (c) 2013 Linaro Ltd.
 * Copyright (c) 2013 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/of_dma.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/delay.h>

#include "virt-dma.h"
#define DMA_CHAN_MASK       0xffff
#define DRIVER_NAME		"hisi-dma64"
#define DMA_ALIGN		3
#define DMA_MAX_SIZE		0x1ffc

#define INT_STAT		0x00
#define INT_TC1			0x04
#define INT_ERR1		0x0c
#define INT_ERR2		0x10
#define INT_TC1_MASK		0x18
#define INT_ERR1_MASK		0x20
#define INT_ERR2_MASK		0x24
#define INT_TC1_RAW		0x600
#define INT_TC2_RAW		0x608
#define INT_ERR1_RAW		0x610
#define INT_ERR2_RAW		0x618
#define CH_PRI			0x688
#define CH_STAT			0x690
#define DMA_CTRL		0x698
#define CX_CUR_CNT		0x404
#define CX_LLI_H		0x804
#define CX_LLI_L		0x800
#define CX_CNT			0x81C
#define CX_SRC_L		0x820
#define CX_SRC_H		0x824
#define CX_DST_L		0x828
#define CX_DST_H		0x82C
#define CX_CONFIG		0x830
#define AXI_CONFIG		0x834

#define CX_LLI_CHAIN_EN		0x2
#define CCFG_EN			0x1
#define CCFG_MEM2PER		(0x1 << 2)
#define CCFG_PER2MEM		(0x2 << 2)
#define CCFG_SRCINCR		(0x1 << 31)
#define CCFG_DSTINCR		(0x1 << 30)
#define GET_HIGH_32BIT(a) ((u32)((u64)(a) >> 32))
#define GET_LOW_32BIT(a) ((u32)((u64)(a) & 0xffffffff))
#define U32_TO_U64(u32h, u32l) ((u64)(u32l) | ((u64)(u32h) << 32))

struct hisi_desc_hw {
	u64 lli;
	u32 reserved1[5];
	u32 count;
	u64 saddr;
	u64 daddr;
	u32 config;
	u32 reserved2[3];
} __aligned(64);

struct hisi_dma_desc_sw {
	struct virt_dma_desc	vd;
	dma_addr_t		desc_hw_lli;
	size_t			desc_num;
	size_t			size;
	struct hisi_desc_hw	desc_hw[0];
};

struct hisi_dma_phy;

struct hisi_dma_chan {
	u32			ccfg;
	struct virt_dma_chan	vc;
	struct hisi_dma_phy	*phy;
	struct list_head	node;
	enum dma_transfer_direction dir;
	dma_addr_t		dev_addr;
	enum dma_status		status;
};

struct hisi_dma_phy {
	u32			idx;
	void __iomem		*base;
	struct hisi_dma_chan	*vchan;
	struct hisi_dma_desc_sw	*ds_run;
	struct hisi_dma_desc_sw	*ds_done;
};

struct hisi_dma_dev {
	struct dma_device	slave;
	void __iomem		*base;
	struct tasklet_struct	task;
	spinlock_t		lock;
	struct list_head	chan_pending;
	struct hisi_dma_phy	*phy;
	struct hisi_dma_chan	*chans;
	struct clk		*clk;
	u32			dma_channels;
	u32			dma_requests;
	u32         dma_min_chan;
};

#define to_hisi_dma(dmadev) container_of(dmadev, struct hisi_dma_dev, slave)

static struct hisi_dma_chan *to_hisi_chan(struct dma_chan *chan) {
	return container_of(chan, struct hisi_dma_chan, vc.chan);
}

static void hisi_dma_pause_dma(struct hisi_dma_phy *phy, struct hisi_dma_dev *d, bool on)
{
	u32 val = 0;
	int timeout;
	if (on) {
		val = (u32)readl(phy->base + CX_CONFIG);
		val |= CCFG_EN;
		writel(val, phy->base + CX_CONFIG);
	} else {
		val = (u32)readl(phy->base + CX_CONFIG);
		val &= ~CCFG_EN;
		writel(val, phy->base + CX_CONFIG);
		/* Wait for channel inactive */
		for (timeout = 2000; timeout > 0; timeout--) {
			if (!(BIT(phy->idx) & (u64)readl(d->base + CH_STAT)))
				break;
			writel(val, phy->base + CX_CONFIG);
			udelay(1);
		}

		if (timeout == 0)
			printk(KERN_ERR ":channel%u timeout waiting for pause, timeout:%d\n",
			       phy->idx, timeout);
	}
}

static void hisi_dma_terminate_chan(struct hisi_dma_phy *phy, struct hisi_dma_dev *d)
{
	u32 val;

	hisi_dma_pause_dma(phy, d, false);

	val = (u32)0x1 << phy->idx;
	writel(val, d->base + INT_TC1_RAW);
	writel(val, d->base + INT_ERR1_RAW);
	writel(val, d->base + INT_ERR2_RAW);
}

static void hisi_dma_set_desc(struct hisi_dma_phy *phy, struct hisi_desc_hw *hw)
{
	writel(GET_HIGH_32BIT(hw->lli), phy->base + CX_LLI_H);
	writel(GET_LOW_32BIT(hw->lli), phy->base + CX_LLI_L);
	writel(hw->count, phy->base + CX_CNT);
	writel(GET_HIGH_32BIT(hw->saddr), phy->base + CX_SRC_H);
	writel(GET_LOW_32BIT(hw->saddr), phy->base + CX_SRC_L);
	writel(GET_HIGH_32BIT(hw->daddr), phy->base + CX_DST_H);
	writel(GET_LOW_32BIT(hw->daddr), phy->base + CX_DST_L);
	writel(hw->config, phy->base + CX_CONFIG);
}

static u32 hisi_dma_get_curr_cnt(struct hisi_dma_dev *d, struct hisi_dma_phy *phy)
{
	u32 cnt;

	cnt = (u32)readl(d->base + CX_CUR_CNT + phy->idx * 0x20);
	cnt &= 0xffff;
	return cnt;
}

static u64 hisi_dma_get_curr_lli(struct hisi_dma_phy *phy)
{
	return U32_TO_U64(readl(phy->base + CX_LLI_H), readl(phy->base + CX_LLI_L));
}

static u32 hisi_dma_get_chan_stat(struct hisi_dma_dev *d)
{
	return readl(d->base + CH_STAT);
}

static void hisi_dma_enable_dma(struct hisi_dma_dev *d, bool on)
{
	if (on) {
		/* set same priority */
		writel(0x0, d ->base + CH_PRI);

		/* unmask irq */
		writel(DMA_CHAN_MASK, d ->base + INT_TC1_MASK);
		writel(DMA_CHAN_MASK, d ->base + INT_ERR1_MASK);
		writel(DMA_CHAN_MASK, d ->base + INT_ERR2_MASK);

	} else {
		/* mask irq */
		writel(0x0, d ->base + INT_TC1_MASK);
		writel(0x0, d ->base + INT_ERR1_MASK);
		writel(0x0, d ->base + INT_ERR2_MASK);
	}
}

static irqreturn_t hisi_dma_int_handler(int irq, void *dev_id)
{
	struct hisi_dma_dev *d = (struct hisi_dma_dev *)dev_id;
	struct hisi_dma_phy *p;
	struct hisi_dma_chan *c;
	u32 stat = (u32)readl(d->base + INT_STAT);
	u32 tc1  = (u32)readl(d->base + INT_TC1);
	u32 err1 = (u32)readl(d->base + INT_ERR1);
	u32 err2 = (u32)readl(d->base + INT_ERR2);
	u32 i, tc1_irq = 0, err1_irq = 0, err2_irq = 0;
	u32 stats = stat;

	while (stat) {
		i = (u32)__ffs((unsigned long)stat);
		stat &= (stat - 1);
		if (likely(tc1 & BIT(i))) {
			p = &d->phy[i];
			c = p->vchan;
			if (c) {
				unsigned long flags;

				spin_lock_irqsave(&c->vc.lock, flags);
				if(p->ds_run != NULL)
					vchan_cookie_complete(&p->ds_run->vd);
				p->ds_done = p->ds_run;
				spin_unlock_irqrestore(&c->vc.lock, flags);
			} else {
				dev_err(d->slave.dev, "%s: phy[%d] stats[0x%x]\n",
						__func__, p->idx, stats);
			}
			tc1_irq |= (u32)BIT(i);
		}

		if (unlikely((err1 & BIT(i)) || (err2 & BIT(i)))) {
			p = &d->phy[i];
			c = p->vchan;
			if (c)
				c->status = DMA_ERROR;
			if(err1 & BIT(i))
				err1_irq |= (u32)BIT(i);
			if(err2 & BIT(i))
				err2_irq |= (u32)BIT(i);
			dev_warn(d->slave.dev, "DMA ERR phy[%d] stats[0x%x] err1[0x%x] err2[0x%x]\n",
				p->idx, stats, err1, err2);
		}

	}

	writel(tc1_irq, d->base + INT_TC1_RAW);
	writel(err1_irq, d->base + INT_ERR1_RAW);
	writel(err2_irq, d->base + INT_ERR2_RAW);

	if (tc1_irq || err1_irq || err2_irq) {
		tasklet_schedule(&d->task);

		return IRQ_HANDLED;
	} else
		return IRQ_NONE;
}

static int hisi_dma_start_txd(struct hisi_dma_chan *c)
{
	struct hisi_dma_dev *d = to_hisi_dma(c->vc.chan.device);
	struct virt_dma_desc *vd = vchan_next_desc(&c->vc);


	if (!c->phy)
		return -ENODEV;

	if (BIT(c->phy->idx) & hisi_dma_get_chan_stat(d)) {
        dev_err(d->slave.dev,  "%s: chan[%d] phy[%d] stat[0x%x]\n", __func__,
            c->vc.chan.chan_id, c->phy->idx, hisi_dma_get_chan_stat(d));
		return -EBUSY;
    }

	if (vd) {
		struct hisi_dma_desc_sw *ds =
		    container_of(vd, struct hisi_dma_desc_sw, vd);

		/*
		 * fetch and remove request from vc->desc_issued
		 * so vc->desc_issued only contains desc pending
		 */
		list_del(&ds->vd.node);
		c->phy->ds_run = ds;
		c->phy->ds_done = NULL;
		/* start dma */
		hisi_dma_set_desc(c->phy, &ds->desc_hw[0]);

		return 0;
	}
	c->phy->ds_done = NULL;
	c->phy->ds_run = NULL;
	return -EAGAIN;
}

static void hisi_dma_tasklet(unsigned long arg)
{
	struct hisi_dma_dev *d = (struct hisi_dma_dev *)arg;
	struct hisi_dma_phy *p;
	struct hisi_dma_chan *c, *cn;
	unsigned pch, pch_alloc = 0;
	unsigned long flags;
    int ret = 0;

	/* check new dma request of running channel in vc->desc_issued */
	list_for_each_entry_safe(c, cn, &d->slave.channels, vc.chan.device_node) {
		spin_lock_irqsave(&c->vc.lock,flags);
		p = c->phy;

		if (p && p->ds_done) {
            ret = hisi_dma_start_txd(c);
			if (-EAGAIN == ret) {
				/* No current txd associated with this channel */
				dev_dbg(d->slave.dev, "pchan %u: free\n", p->idx);
				/* Mark this channel free */
				c->phy = NULL;
				p->vchan = NULL;
			} else if (0 != ret) {
                continue;
            }
		} else if(p && c->status == DMA_ERROR) {
			hisi_dma_terminate_chan(p, d);
			c->phy = NULL;
			p->vchan = NULL;
			p->ds_run = p->ds_done = NULL;
		}

		spin_unlock_irqrestore(&c->vc.lock,flags);
	}

	/* check new channel request in d->chan_pending */
	spin_lock_irqsave(&d->lock,flags);

	for (pch = d->dma_min_chan; pch < d->dma_channels; pch++) {
		p = &d->phy[pch];

		if (p->vchan == NULL && !list_empty(&d->chan_pending)) {
			c = list_first_entry(&d->chan_pending,
			                     struct hisi_dma_chan, node);
			/* remove from d->chan_pending */
			list_del_init(&c->node);
			pch_alloc |= (u32)1 << pch;
			/* Mark this channel allocated */
			p->vchan = c;
			c->phy = p;
			dev_dbg(d->slave.dev, "pchan %u: alloc vchan %pK\n", pch, &c->vc);
		}
	}
	spin_unlock_irqrestore(&d->lock,flags);

	for (pch = d->dma_min_chan; pch < d->dma_channels; pch++) {
		if (pch_alloc & ((u32)1 << pch)) {

			p = &d->phy[pch];
			c = p->vchan;
			if (c) {
				spin_lock_irqsave(&c->vc.lock,flags);
				hisi_dma_start_txd(c);
				spin_unlock_irqrestore(&c->vc.lock,flags);
			}
		}
	}
}

static int hisi_dma_alloc_chan_resources(struct dma_chan *chan)
{
	return 0;
}

static void hisi_dma_free_chan_resources(struct dma_chan *chan)
{
	struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_dev *d = to_hisi_dma(chan->device);
	unsigned long flags;

	spin_lock_irqsave(&d->lock, flags);
	list_del_init(&c->node);
	spin_unlock_irqrestore(&d->lock, flags);

	vchan_free_chan_resources(&c->vc);
	c->ccfg = 0;
}

static enum dma_status hisi_dma_tx_status(struct dma_chan *chan,
        dma_cookie_t cookie, struct dma_tx_state *state)
{
	struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_dev *d = to_hisi_dma(chan->device);
	struct hisi_dma_phy *p;
	struct virt_dma_desc *vd;
	unsigned long flags;
	enum dma_status ret;
	size_t bytes = 0;

	ret = dma_cookie_status(&c->vc.chan, cookie, state);
	if (ret == DMA_COMPLETE)
		return ret;

	spin_lock_irqsave(&c->vc.lock, flags);
	p = c->phy;
	ret = c->status;

	/*
	 * If the cookie is on our issue queue, then the residue is
	 * its total size.
	 */
	vd = vchan_find_desc(&c->vc, cookie);
	if (vd) {
		bytes = container_of(vd, struct hisi_dma_desc_sw, vd)->size;
	} else if ((!p) || (!p->ds_run)) {
		bytes = 0;
	} else {
		struct hisi_dma_desc_sw *ds = p->ds_run;
		u64 clli, index;

		bytes = hisi_dma_get_curr_cnt(d, p);
		clli = hisi_dma_get_curr_lli(p);
		index = (clli - ds->desc_hw_lli) / sizeof(struct hisi_desc_hw);
		for (; index < ds->desc_num; index++) {
			bytes += ds->desc_hw[index].count;
			/* end of lli */
			if (!ds->desc_hw[index].lli)
				break;
		}
	}
	spin_unlock_irqrestore(&c->vc.lock, flags);
	dma_set_residue(state, (u32)bytes);
	return ret;
}

static void hisi_dma_issue_pending(struct dma_chan *chan)
{
	struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_dev *d = to_hisi_dma(chan->device);
	unsigned long flags;

	spin_lock_irqsave(&c->vc.lock, flags);
	/* add request to vc->desc_issued */
	if (vchan_issue_pending(&c->vc)) {
		spin_lock(&d->lock);
		if (!c->phy) {
			if (list_empty(&c->node)) {
				/* if new channel, add chan_pending */
				list_add_tail(&c->node, &d->chan_pending);
				/* check in tasklet */
				tasklet_schedule(&d->task);
				dev_dbg(d->slave.dev, "vchan %pK: issued\n", &c->vc);
			}
		}
		spin_unlock(&d->lock);
	} else
		dev_dbg(d->slave.dev, "vchan %pK: nothing to issue\n", &c->vc);
	spin_unlock_irqrestore(&c->vc.lock, flags);
}

static void hisi_dma_fill_desc(struct hisi_dma_desc_sw *ds, dma_addr_t dst,
                               dma_addr_t src, size_t len, u32 num, u32 ccfg)
{
	if ((num + 1) < ds->desc_num)
		ds->desc_hw[num].lli = ds->desc_hw_lli + (u64)(num + 1) *
		                       sizeof(struct hisi_desc_hw);
	ds->desc_hw[num].lli |= CX_LLI_CHAIN_EN;
	ds->desc_hw[num].count = (u32)len;
	ds->desc_hw[num].saddr = src;
	ds->desc_hw[num].daddr = dst;
	ds->desc_hw[num].config = ccfg;
}

static struct dma_async_tx_descriptor *hisi_dma_prep_memcpy(
    struct dma_chan *chan,	dma_addr_t dst, dma_addr_t src,
    size_t len, unsigned long flags) {
	struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_desc_sw *ds;
	size_t copy = 0;
	u32 num;

	if (!len)
		return NULL;

	num = (u32)DIV_ROUND_UP(len, DMA_MAX_SIZE);
	ds = kzalloc(sizeof(*ds) + num * sizeof(ds->desc_hw[0]), GFP_ATOMIC | GFP_DMA);
	if (!ds) {
		dev_dbg(chan->device->dev, "vchan %pK: kzalloc fail\n", (void*)&c->vc);
		return NULL;
	}
	ds->desc_hw_lli = __virt_to_phys((unsigned long)&ds->desc_hw[0]);
	ds->size = len;
	ds->desc_num = (size_t)num;
	num = 0;

	if (!c->ccfg) {
		/* default is memtomem, without calling device_control */
		c->ccfg = CCFG_SRCINCR | CCFG_DSTINCR | CCFG_EN;
		c->ccfg |= (0xf << 20) | (0xf << 24);	/* burst = 16 */
		c->ccfg |= (0x3 << 12) | (0x3 << 16);	/* width = 64 bit */
	}

	do {
		copy = min_t(size_t, len, DMA_MAX_SIZE);
		hisi_dma_fill_desc(ds, dst, src, copy, num++, c->ccfg);

		if (c->dir == DMA_MEM_TO_DEV) {
			src += copy;
		} else if (c->dir == DMA_DEV_TO_MEM) {
			dst += copy;
		} else {
			src += copy;
			dst += copy;
		}
		len -= copy;
	} while (len);

	ds->desc_hw[num-1].lli = 0;	/* end of link */

	return vchan_tx_prep(&c->vc, &ds->vd, flags);
}

static struct dma_async_tx_descriptor *hisi_dma_prep_slave_sg(
    struct dma_chan *chan, struct scatterlist *sgl, unsigned int sglen,
    enum dma_transfer_direction dir, unsigned long flags, void *context) {
	struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_desc_sw *ds;
	size_t len, avail, total = 0;
	struct scatterlist *sg;
	dma_addr_t addr, src = 0, dst = 0;
	u32 num = sglen, i;

	if (sgl == NULL)
		return NULL;

	for_each_sg(sgl, sg, sglen, i) {
		avail = sg_dma_len(sg);
		if (avail > DMA_MAX_SIZE)
			num += (u32)DIV_ROUND_UP(avail, DMA_MAX_SIZE) - 1;
	}

	ds = kzalloc(sizeof(*ds) + num * sizeof(ds->desc_hw[0]), GFP_ATOMIC | GFP_DMA);
	if (!ds) {
		dev_dbg(chan->device->dev, "vchan %pK: kzalloc fail\n", &c->vc);
		return NULL;
	}
	ds->desc_hw_lli = __virt_to_phys((unsigned long)&ds->desc_hw[0]);
	ds->desc_num = num;
	num = 0;

	for_each_sg(sgl, sg, sglen, i) {
		addr = sg_dma_address(sg);
		avail = sg_dma_len(sg);
		total += avail;

		do {
			len = min_t(size_t, avail, DMA_MAX_SIZE);

			if (dir == DMA_MEM_TO_DEV) {
				src = addr;
				dst = c->dev_addr;
			} else if (dir == DMA_DEV_TO_MEM) {
				src = c->dev_addr;
				dst = addr;
			}

			hisi_dma_fill_desc(ds, dst, src, len, num++, c->ccfg);

			addr += len;
			avail -= len;
		} while (avail);
	}

	ds->desc_hw[(int)num-1].lli = 0;	/* end of link */

	ds->size = total;
	return vchan_tx_prep(&c->vc, &ds->vd, flags);
}

static int hisi_dma_config(struct dma_chan *chan, struct dma_slave_config *config)
{
    struct hisi_dma_chan *c = to_hisi_chan(chan);
    struct hisi_dma_dev *d = to_hisi_dma(chan->device);
    u32 maxburst = 0, val = 0;
    enum dma_slave_buswidth width = DMA_SLAVE_BUSWIDTH_UNDEFINED;

    if (NULL == c) {
		dev_warn(d->slave.dev, "vchan is NULL\n");
		return -EINVAL;
	}

    if (config == NULL)
			return -EINVAL;
		c->dir = config->direction;
		if (c->dir == DMA_DEV_TO_MEM) {
			c->ccfg = CCFG_DSTINCR;
			c->dev_addr = config->src_addr;
			maxburst = config->src_maxburst;
			width = config->src_addr_width;
		} else if (c->dir == DMA_MEM_TO_DEV) {
			c->ccfg = CCFG_SRCINCR;
			c->dev_addr = config->dst_addr;
			maxburst = config->dst_maxburst;
			width = config->dst_addr_width;
		} else if(c->dir == DMA_MEM_TO_MEM) {
			c->ccfg = CCFG_SRCINCR | CCFG_DSTINCR;
		}

		switch (width) {
		case DMA_SLAVE_BUSWIDTH_1_BYTE:
			val = 0;
			break;
		case DMA_SLAVE_BUSWIDTH_2_BYTES:
			val = 1;
			break;
		case DMA_SLAVE_BUSWIDTH_4_BYTES:
			val = 2;
			break;
		case DMA_SLAVE_BUSWIDTH_8_BYTES:
			val = 3;
			break;
		default:
			break;
		}
		c->ccfg |= (val << 12) | (val << 16);

		if ((maxburst == 0) || (maxburst > 16))
			val = 16;
		else
			val = maxburst - 1;
		c->ccfg |= (val << 20) | (val << 24);
		c->ccfg |= CCFG_MEM2PER | CCFG_EN;

		/* specific request line */
		c->ccfg |= (u32)c->vc.chan.chan_id << 4;
		return 0;
}

static int hisi_dma_pause(struct dma_chan *chan)
{
    struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_dev *d = to_hisi_dma(chan->device);
	unsigned long flags;
	struct hisi_dma_phy *p = NULL;

    if (NULL == c) {
		dev_warn(d->slave.dev, "vchan is NULL\n");
		return -EINVAL;
	}

    dev_dbg(d->slave.dev, "vchan %pK: pause\n", &c->vc);
	if (c->status == DMA_IN_PROGRESS) {
		c->status = DMA_PAUSED;
		spin_lock_irqsave(&d->lock,flags);
		p = c->phy;
		if (p) {
			spin_unlock_irqrestore(&d->lock,flags);
			hisi_dma_pause_dma(p, d, false);
		} else {
			list_del_init(&c->node);
			spin_unlock_irqrestore(&d->lock,flags);
		}
	}
    return 0;
}

static int hisi_dma_resume(struct dma_chan *chan)
{
    struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_dev *d = to_hisi_dma(chan->device);
	unsigned long flags;
	struct hisi_dma_phy *p;

    if (NULL == c) {
		dev_warn(d->slave.dev, "vchan is NULL\n");
		return -EINVAL;
	}

    dev_dbg(d->slave.dev, "vchan %pK: resume\n", &c->vc);
	spin_lock_irqsave(&c->vc.lock, flags);
	p = c->phy;
	if (c->status == DMA_PAUSED) {
		c->status = DMA_IN_PROGRESS;
		if (p) {
			hisi_dma_pause_dma(p, d, true);
		} else if (!list_empty(&c->vc.desc_issued)) {
			spin_lock(&d->lock);
			list_add_tail(&c->node, &d->chan_pending);
			spin_unlock(&d->lock);
		}
	}
	spin_unlock_irqrestore(&c->vc.lock, flags);
    return 0;
}

static int hisi_dma_terminate_all(struct dma_chan *chan)
{
    struct hisi_dma_chan *c = to_hisi_chan(chan);
	struct hisi_dma_dev *d = to_hisi_dma(chan->device);
	unsigned long flags;
	struct hisi_dma_phy *p;
    LIST_HEAD(head);

    if (NULL == c) {
		dev_warn(d->slave.dev, "vchan is NULL\n");
		return -EINVAL;
	}

    dev_dbg(d->slave.dev, "vchan %pK: terminate all\n", &c->vc);
	/* Prevent this channel being scheduled */
	spin_lock_irqsave(&d->lock,flags);
	list_del_init(&c->node);
	spin_unlock_irqrestore(&d->lock,flags);

	/* Clear the tx descriptor lists */
	spin_lock_irqsave(&c->vc.lock, flags);
	p = c->phy;
	vchan_get_all_descriptors(&c->vc, &head);
	if(p && p->ds_run != NULL && p->ds_done == NULL) {
		list_add_tail(&p->ds_run->vd.node, &head);
	}

	if (p) {
		/* vchan is assigned to a pchan - stop the channel */
		hisi_dma_terminate_chan(p, d);
		c->phy = NULL;
		p->vchan = NULL;
		p->ds_run = p->ds_done = NULL;
	}
	spin_unlock_irqrestore(&c->vc.lock, flags);
	vchan_dma_desc_free_list(&c->vc, &head);

    return 0;
}

static void hisi_dma_free_desc(struct virt_dma_desc *vd)
{
	struct hisi_dma_desc_sw *ds =
	    container_of(vd, struct hisi_dma_desc_sw, vd);

	kfree(ds);
}

static struct of_device_id hisi_pdma64_dt_ids[] = {
	{ .compatible = "hisilicon,hisi-dma64-1.0", },
	{}
};
MODULE_DEVICE_TABLE(of, hisi_pdma64_dt_ids);

static struct dma_chan *hisi_of_dma_simple_xlate(struct of_phandle_args *dma_spec,
        struct of_dma *ofdma) {
	struct hisi_dma_dev *d = ofdma->of_dma_data;
	unsigned int request = dma_spec->args[0];

	if (request > d->dma_requests)
		return NULL;

	return dma_get_slave_channel(&(d->chans[request].vc.chan));
}

static int hisi_dma_probe(struct platform_device *op)
{
	struct hisi_dma_dev *d;
	const struct of_device_id *of_id;
	struct resource *iores;
	u32 i, irq;
    int ret = 0;

	iores = platform_get_resource(op, IORESOURCE_MEM, 0);
	if (!iores)
		return -EINVAL;

	d = devm_kzalloc(&op->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->base = devm_ioremap_resource(&op->dev, iores);
	if (!d->base)
		return -EADDRNOTAVAIL;

	of_id = of_match_device(hisi_pdma64_dt_ids, &op->dev);
	if (of_id) {
		ret = of_property_read_u32((&op->dev)->of_node,
		                           "dma-channels", &d->dma_channels);
	}
	if (ret) {
		dev_err(&op->dev,"%s doesn't have dma-channels property!\n",
		        __func__);
		return ret;
	}
	ret = of_property_read_u32((&op->dev)->of_node,
	                           "dma-requests", &d->dma_requests);
	if (ret) {
		dev_err(&op->dev,"%s doesn't have dma-request property!\n",
		        __func__);
		return ret;
	}

	of_property_read_u32((&op->dev)->of_node,
		"dma-min-chan", &d->dma_min_chan);

	dev_info(&op->dev, "dma_channels:0x%x dma_requests:0x%x dma_min_chan:0x%x \n",
		d->dma_channels,d->dma_requests,d->dma_min_chan);

	op->dev.dma_mask = &(op->dev.coherent_dma_mask);
	op->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	d->clk = devm_clk_get(&op->dev, NULL);
	if (IS_ERR(d->clk)) {
		dev_err(&op->dev, "no dma clk\n");
		return (int)PTR_ERR(d->clk);
	}

	irq = (u32)platform_get_irq(op, 0);
	ret = devm_request_irq(&op->dev, irq,
	                       hisi_dma_int_handler, 0, DRIVER_NAME, d);
	if (ret)
		return ret;

	/* init phy channel */
	d->phy = devm_kzalloc(&op->dev,
	                      d->dma_channels * sizeof(struct hisi_dma_phy), GFP_KERNEL);
	if (d->phy == NULL)
		return -ENOMEM;

	for (i = d->dma_min_chan; i < d->dma_channels; i++) {
		struct hisi_dma_phy *p = &d->phy[i];
		p->idx = i;
		p->base = d->base + i * 0x40;
	}

	INIT_LIST_HEAD(&d->slave.channels);
	dma_cap_set(DMA_SLAVE, d->slave.cap_mask);
	dma_cap_set(DMA_MEMCPY, d->slave.cap_mask);
	dma_cap_set(DMA_PRIVATE, d->slave.cap_mask);
	op->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	op->dev.dma_mask = &op->dev.coherent_dma_mask;
	d->slave.dev = &op->dev;
	d->slave.device_alloc_chan_resources = hisi_dma_alloc_chan_resources;
	d->slave.device_free_chan_resources = hisi_dma_free_chan_resources;
	d->slave.device_tx_status = hisi_dma_tx_status;
	d->slave.device_prep_dma_memcpy = hisi_dma_prep_memcpy;
	d->slave.device_prep_slave_sg = hisi_dma_prep_slave_sg;
	d->slave.device_issue_pending = hisi_dma_issue_pending;
	d->slave.device_config = hisi_dma_config;
	d->slave.device_pause = hisi_dma_pause;
	d->slave.device_resume = hisi_dma_resume;
	d->slave.device_terminate_all = hisi_dma_terminate_all;
	d->slave.copy_align = DMA_ALIGN;
	d->slave.chancnt = d->dma_requests;

	/* init virtual channel */
	d->chans = devm_kzalloc(&op->dev,
	                        d->dma_requests * sizeof(struct hisi_dma_chan), GFP_KERNEL);
	if (d->chans == NULL)
		return -ENOMEM;

	for (i = 0; i < d->dma_requests; i++) {
		struct hisi_dma_chan *c = &d->chans[i];

		c->status = DMA_IN_PROGRESS;
		INIT_LIST_HEAD(&c->node);
		c->vc.desc_free = hisi_dma_free_desc;
		vchan_init(&c->vc, &d->slave);
	}

	/* Enable clock before accessing registers */
	ret = clk_prepare_enable(d->clk);
	if (ret < 0) {
		dev_err(&op->dev, "clk_prepare_enable failed: %d\n", ret);
		return -EINVAL;
	}
	hisi_dma_enable_dma(d, true);

	ret = dma_async_device_register(&d->slave);
	if (ret)
		return ret;

	ret = of_dma_controller_register((&op->dev)->of_node,
	                                 hisi_of_dma_simple_xlate, d);
	if (ret)
		goto of_dma_register_fail;

	spin_lock_init(&d->lock);
	INIT_LIST_HEAD(&d->chan_pending);
	tasklet_init(&d->task, hisi_dma_tasklet, (unsigned long)d);
	platform_set_drvdata(op, d);
	dev_info(&op->dev, "initialized\n");


	dev_info(&op->dev, "hisi_dma64_probe  ok!\n");

	return 0;

of_dma_register_fail:
	dma_async_device_unregister(&d->slave);
	return ret;
}

static int hisi_dma_remove(struct platform_device *op)
{
	struct hisi_dma_chan *c, *cn;
	struct hisi_dma_dev *d = platform_get_drvdata(op);

	if (!d){
		pr_err("%s: get drvdata failed\n", __func__);
		return -EINVAL;
	}

	dma_async_device_unregister(&d->slave);
	of_dma_controller_free((&op->dev)->of_node);

	list_for_each_entry_safe(c, cn, &d->slave.channels, vc.chan.device_node) {
		list_del(&c->vc.chan.device_node);
		tasklet_kill(&c->vc.task);
	}
	tasklet_kill(&d->task);

	clk_disable_unprepare(d->clk);

	return 0;
}

#if defined(CONFIG_PM_SLEEP)
static int hisi_dma_pltfm_suspend(struct device *dev)
{
	struct hisi_dma_dev *d = dev_get_drvdata(dev);
	u32 stat;
	u32 limit = 500;

        if (!d){
                pr_err("%s: get drvdata failed\n", __func__);
                return -EINVAL;
        }

	dev_info(d->slave.dev, "%s: suspend +\n", __func__);
	pm_runtime_get_sync(d->slave.dev);

	stat = hisi_dma_get_chan_stat(d);
	stat = stat & DMA_CHAN_MASK;

	while ( (stat) && limit--) {
                udelay(1000);
                stat = hisi_dma_get_chan_stat(d);
                stat = stat & DMA_CHAN_MASK;
	}

	if (stat) {
		dev_warn(d->slave.dev,"chan 0x%x is running fail to suspend\n", stat);
		{
			u32 i;
			for(i = d->dma_min_chan; i < d->dma_channels; i++) {
			dev_warn(d->slave.dev,"chanel:%d, CX_CONFIG: 0x%x, CX_AXI_CONF: 0x%x\n",i,
				readl(d->base + CX_CONFIG + i*0x40), readl(d->base + AXI_CONFIG + i*0x40));
			}
		}
		return -1;
	}

	hisi_dma_enable_dma(d, false);

	clk_disable(d->clk);

	pm_runtime_put_sync(d->slave.dev);
	dev_info(d->slave.dev, "%s: suspend -\n", __func__);
	return 0;
}

static int hisi_dma_pltfm_resume(struct device *dev)
{
	struct hisi_dma_dev *d = dev_get_drvdata(dev);
        int ret = 0;

        if (!d){
                pr_err("%s: get drvdata failed\n", __func__);
                return -EINVAL;
        }

	dev_info(d->slave.dev, "%s: resume +\n", __func__);
	pm_runtime_get_sync(d->slave.dev);


       ret = clk_enable(d->clk);
       if (ret < 0) {
               dev_err(d->slave.dev, "clk_prepare_enable failed: %d\n", ret);
                return -EINVAL;
       }

	hisi_dma_enable_dma(d, true);

	pm_runtime_put_sync(d->slave.dev);
	dev_info(d->slave.dev, "%s: resume -\n", __func__);
	return 0;
}
#else
#define hisi_dma_pltfm_suspend	NULL
#define hisi_dma_pltfm_resume	NULL
#endif /* CONFIG_PM_SLEEP*/

//SIMPLE_DEV_PM_OPS(hisi_dma_pltfm_pmops, hisi_dma_pltfm_suspend, hisi_dma_pltfm_resume);


const struct dev_pm_ops hisi_dma64_pm_ops = {
	.suspend_late = hisi_dma_pltfm_suspend,
	.resume_early	= hisi_dma_pltfm_resume,
};


static struct platform_driver hisi_pdma64_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner  = THIS_MODULE,
		.pm	= &hisi_dma64_pm_ops,
		.of_match_table = hisi_pdma64_dt_ids,
	},
	.probe		= hisi_dma_probe,
	.remove		= hisi_dma_remove,
};

static int __init dmac_module_init(void)
{
	int retval;

	retval = platform_driver_register(&hisi_pdma64_driver);
	if (retval) {
		printk(KERN_ERR "hisidma platform driver register failed\n");
		return retval;
	}

	return retval;
}

static void __exit dmac_module_exit(void)
{
	platform_driver_unregister(&hisi_pdma64_driver);
}

arch_initcall(dmac_module_init);
MODULE_DESCRIPTION("Hisilicon hisi 64bit DMA Driver");
MODULE_LICENSE("GPL v2");


/*
 * Hisilicon's Hi3660 mailbox driver
 *
 * Copyright (c) 2017 Hisilicon Limited.
 * Copyright (c) 2017 Linaro Limited.
 *
 * Author: Leo Yan <leo.yan@linaro.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kfifo.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "mailbox.h"

#define MBOX_CHAN_MAX			32

#define MBOX_TX				0x1

/* Mailbox message length: 2 words */
#define MBOX_MSG_LEN			2

#define MBOX_OFF(m)			(0x40 * (m))
#define MBOX_SRC_REG(m)			MBOX_OFF(m)
#define MBOX_DST_REG(m)			(MBOX_OFF(m) + 0x04)
#define MBOX_DCLR_REG(m)		(MBOX_OFF(m) + 0x08)
#define MBOX_DSTAT_REG(m)		(MBOX_OFF(m) + 0x0C)
#define MBOX_MODE_REG(m)		(MBOX_OFF(m) + 0x10)
#define MBOX_IMASK_REG(m)		(MBOX_OFF(m) + 0x14)
#define MBOX_ICLR_REG(m)		(MBOX_OFF(m) + 0x18)
#define MBOX_SEND_REG(m)		(MBOX_OFF(m) + 0x1C)
#define MBOX_DATA_REG(m, i)		(MBOX_OFF(m) + 0x20 + ((i) << 2))

#define MBOX_CPU_IMASK(cpu)		(((cpu) << 3) + 0x800)
#define MBOX_CPU_IRST(cpu)		(((cpu) << 3) + 0x804)
#define MBOX_IPC_LOCK			(0xA00)

#define MBOX_IPC_UNLOCKED		0x00000000
#define AUTOMATIC_ACK_CONFIG		(1 << 0)
#define NO_FUNC_CONFIG			(0 << 0)

#define MBOX_MANUAL_ACK			0
#define MBOX_AUTO_ACK			1

#define MBOX_STATE_IDLE			(1 << 4)
#define MBOX_STATE_OUT			(1 << 5)
#define MBOX_STATE_IN			(1 << 6)
#define MBOX_STATE_ACK			(1 << 7)

#define MBOX_DESTINATION_STATUS		(1 << 6)

struct hi3660_mbox_chan {

	/*
	 * Description for channel's hardware info:
	 *  - direction: tx or rx
	 *  - dst irq: peer core's irq number
	 *  - ack irq: local irq number
	 *  - slot number
	 */
	unsigned int dir, dst_irq, ack_irq;
	unsigned int slot;

	unsigned int *buf;

	unsigned int irq_mode;

	struct hi3660_mbox *parent;
};

struct hi3660_mbox {
	struct device *dev;

	int irq;

	/* flag of enabling tx's irq mode */
	bool tx_irq_mode;

	/* region for mailbox */
	void __iomem *base;

	unsigned int chan_num;
	struct hi3660_mbox_chan *mchan;

	void *irq_map_chan[MBOX_CHAN_MAX];
	struct mbox_chan *chan;
	struct mbox_controller controller;
};

static inline void __ipc_lock(void __iomem *base, unsigned int lock_key)
{
	pr_debug("%s: base %p key %d\n", __func__, base, lock_key);

	__raw_writel(lock_key, base + MBOX_IPC_LOCK);
}

static inline void __ipc_unlock(void __iomem *base, unsigned int key)
{
	pr_debug("%s: base %p key %d\n", __func__, base, key);

	__raw_writel(key, base + MBOX_IPC_LOCK);
}

static inline unsigned int __ipc_lock_status(void __iomem *base)
{
	pr_debug("%s: base %p status %d\n",
		__func__, base, __raw_readl(base + MBOX_IPC_LOCK));

	return __raw_readl(base + MBOX_IPC_LOCK);
}

static inline void __ipc_set_src(void __iomem *base, int source, int mdev)
{
	pr_debug("%s: base %p src %x mdev %d\n",
		__func__, base, source, mdev);

	__raw_writel(BIT(source), base + MBOX_SRC_REG(mdev));
}

static inline unsigned int __ipc_read_src(void __iomem *base, int mdev)
{
	pr_debug("%s: base %p src %x mdev %d\n",
		__func__, base, __raw_readl(base + MBOX_SRC_REG(mdev)), mdev);

	return __raw_readl(base + MBOX_SRC_REG(mdev));
}

static inline void __ipc_set_des(void __iomem *base, int source, int mdev)
{
	pr_debug("%s: base %p src %x mdev %d\n",
		__func__, base, source, mdev);

	__raw_writel(BIT(source), base + MBOX_DST_REG(mdev));
}

static inline void __ipc_clr_des(void __iomem *base, int source, int mdev)
{
	pr_debug("%s: base %p src %x mdev %d\n",
		__func__, base, source, mdev);

	__raw_writel(BIT(source), base + MBOX_DCLR_REG(mdev));
}

static inline unsigned int __ipc_des_status(void __iomem *base, int mdev)
{
	pr_debug("%s: base %p src %x mdev %d\n",
		__func__, base, __raw_readl(base + MBOX_DSTAT_REG(mdev)), mdev);

	return __raw_readl(base + MBOX_DSTAT_REG(mdev));
}

static inline void __ipc_send(void __iomem *base, unsigned int tosend, int mdev)
{
	pr_debug("%s: base %p tosend %x mdev %d\n",
		__func__, base, tosend, mdev);

	__raw_writel(tosend, base + MBOX_SEND_REG(mdev));
}

static inline unsigned int __ipc_read(void __iomem *base, int mdev, int index)
{
	pr_debug("%s: base %p index %d data %x mdev %d\n",
		__func__, base, index, __raw_readl(base + MBOX_DATA_REG(mdev, index)), mdev);

	return __raw_readl(base + MBOX_DATA_REG(mdev, index));
}

static inline void __ipc_write(void __iomem *base, u32 data, int mdev, int index)
{
	pr_debug("%s: base %p index %d data %x mdev %d\n",
		__func__, base, index, data, mdev);

	__raw_writel(data, base + MBOX_DATA_REG(mdev, index));
}

static inline unsigned int __ipc_cpu_imask_get(void __iomem *base, int mdev)
{
	pr_debug("%s: base %p imask %x mdev %d\n",
		__func__, base, __raw_readl(base + MBOX_IMASK_REG(mdev)), mdev);

	return __raw_readl(base + MBOX_IMASK_REG(mdev));
}

static inline void __ipc_cpu_imask_clr(void __iomem *base, unsigned int toclr, int mdev)
{
	unsigned int reg;

	pr_debug("%s: base %p toclr %x mdev %d\n",
		__func__, base, toclr, mdev);

	reg = __raw_readl(base + MBOX_IMASK_REG(mdev));
	reg = reg & (~(toclr));

	__raw_writel(reg, base + MBOX_IMASK_REG(mdev));
}

static inline void __ipc_cpu_imask_all(void __iomem *base, int mdev)
{
	pr_debug("%s: base %p mdev %d\n", __func__, base, mdev);

	__raw_writel((~0), base + MBOX_IMASK_REG(mdev));
}

static inline void __ipc_cpu_iclr(void __iomem *base, unsigned int toclr, int mdev)
{
	pr_debug("%s: base %p toclr %x mdev %d\n",
		__func__, base, toclr, mdev);

	__raw_writel(toclr, base + MBOX_ICLR_REG(mdev));
}

static inline int __ipc_cpu_istatus(void __iomem *base, int mdev)
{
	pr_debug("%s: base %p mdev %d\n", __func__, base, mdev);

	return __raw_readl(base + MBOX_ICLR_REG(mdev));
}

static inline unsigned int __ipc_mbox_istatus(void __iomem *base, int cpu)
{
	pr_debug("%s: base %p cpu %d\n", __func__, base, cpu);

	return __raw_readl(base + MBOX_CPU_IMASK(cpu));
}

static inline unsigned int __ipc_mbox_irstatus(void __iomem *base, int cpu)
{
	pr_debug("%s: base %p cpu %d\n", __func__, base, cpu);

	return __raw_readl(base + MBOX_CPU_IRST(cpu));
}

static inline unsigned int __ipc_status(void __iomem *base, int mdev)
{
	pr_debug("%s: base %p mdev %d status %x\n",
		__func__, base, mdev, __raw_readl(base + MBOX_MODE_REG(mdev)));

	return __raw_readl(base + MBOX_MODE_REG(mdev));
}

static inline void __ipc_mode(void __iomem *base, unsigned int mode, int mdev)
{
	pr_debug("%s: base %p mdev %d mode %u\n",
		__func__, base, mdev, mode);

	__raw_writel(mode, base + MBOX_MODE_REG(mdev));
}

static int _mdev_check_state_machine(struct hi3660_mbox_chan *mchan,
				     unsigned int state)
{
	struct hi3660_mbox *mbox = mchan->parent;
	int is_same = 0;

	if ((state & __ipc_status(mbox->base, mchan->slot)))
		is_same = 1;

	pr_debug("%s: stm %u ch %d is_stm %d\n", __func__,
		state, mchan->slot, is_same);

	return is_same;
}

static void _mdev_release(struct hi3660_mbox_chan *mchan)
{
	struct hi3660_mbox *mbox = mchan->parent;

	__ipc_cpu_imask_all(mbox->base, mchan->slot);
	__ipc_set_src(mbox->base, mchan->ack_irq, mchan->slot);

	asm volatile ("sev");
	return;
}

static void _mdev_ensure_channel(struct hi3660_mbox_chan *mchan)
{
	int timeout = 0, loop = 60 + 1000;

	if (_mdev_check_state_machine(mchan, MBOX_STATE_IDLE))
		/*IDLE STATUS, return directly */
		return;

	if (_mdev_check_state_machine(mchan, MBOX_STATE_ACK))
		/*ACK STATUS, release the channel directly */
		goto release;

	/*
	* The worst situation is to delay:
	* 1000 * 5us + 60 * 5ms = 305ms
	*/
	while (timeout < loop) {

		if (timeout < 1000)
			udelay(5);
		else
			usleep_range(3000, 5000);

		/* If the ack status is ready, bail out */
		if (_mdev_check_state_machine(mchan, MBOX_STATE_ACK))
			break;

		timeout++;
	}

	if (unlikely(timeout == loop)) {
		printk("\n %s ipc timeout...\n", __func__);

		/* TODO: add dump function */
	}

release:
	/*release the channel */
	_mdev_release(mchan);
}

static int _mdev_unlock(struct hi3660_mbox_chan *mchan)
{
	struct hi3660_mbox *mbox = mchan->parent;
	int retry = 3;

	do {
		__ipc_unlock(mbox->base, 0x1ACCE551);
		if (MBOX_IPC_UNLOCKED == __ipc_lock_status(mbox->base))
			break;

		udelay(10);
		retry--;
	} while (retry);

	if (!retry)
		return -ENODEV;

	return 0;
}

static int _mdev_occupy(struct hi3660_mbox_chan *mchan)
{
	struct hi3660_mbox *mbox = mchan->parent;
	unsigned int slot = mchan->slot;
	int retry = 10;

	do {
		/*
		 * hardware locking for exclusive accesing between
		 * CPUs without exclusive monitor mechanism.
		 */
		if (!(__ipc_status(mbox->base, slot) & MBOX_STATE_IDLE))
			__asm__ volatile ("wfe");
		else {
			/* Set the source processor bit */
			__ipc_set_src(mbox->base, mchan->ack_irq, slot);
			if (__ipc_read_src(mbox->base, slot) & BIT(mchan->ack_irq))
				break;
		}

		retry--;
		/* Hardware unlock */
	} while (retry);

	if (!retry)
		return -ENODEV;

	return 0;
}

static bool hi3660_mbox_last_tx_done(struct mbox_chan *chan)
{
	return 1;
}

static int _mdev_hw_send(struct hi3660_mbox_chan *mchan, u32 *msg, u32 len)
{
	struct hi3660_mbox *mbox = mchan->parent;
	int i, ack_mode;
	unsigned int temp;

	if (mchan->irq_mode)
		ack_mode = MBOX_MANUAL_ACK;
	else
		ack_mode = MBOX_AUTO_ACK;

	/* interrupts unmask */
	__ipc_cpu_imask_all(mbox->base, mchan->slot);

	if (MBOX_AUTO_ACK == ack_mode)
		temp = BIT(mchan->dst_irq);
	else
		temp = BIT(mchan->ack_irq) | BIT(mchan->dst_irq);

	__ipc_cpu_imask_clr(mbox->base, temp, mchan->slot);

	/* des config */
	__ipc_set_des(mbox->base, mchan->dst_irq, mchan->slot);

	/* ipc mode config */
	if (MBOX_AUTO_ACK == ack_mode)
		temp = AUTOMATIC_ACK_CONFIG;
	else
		temp = NO_FUNC_CONFIG;

	__ipc_mode(mbox->base, temp, mchan->slot);

	/* write data */
	for (i = 0; i < len; i++)
		__ipc_write(mbox->base, msg[i], mchan->slot, i);

	mchan->buf = msg;

	/* enable sending */
	__ipc_send(mbox->base, BIT(mchan->ack_irq), mchan->slot);
	return 0;
}

static int hi3660_mbox_send_data(struct mbox_chan *chan, void *msg)
{
	struct hi3660_mbox_chan *mchan = chan->con_priv;
	int err = 0;

	/* indicate as a TX channel */
	mchan->dir = MBOX_TX;

	_mdev_ensure_channel(mchan);

	if (_mdev_unlock(mchan)) {
		pr_err("%s: can not be unlocked\n", __func__);
		err = -EIO;
		goto out;
	}

	if (_mdev_occupy(mchan)) {
		pr_err("%s: can not be occupied\n", __func__);
		err = -EBUSY;
		goto out;
	}

	_mdev_hw_send(mchan, msg, MBOX_MSG_LEN);

out:
	return err;
}

static irqreturn_t hi3660_mbox_interrupt(int irq, void *p)
{
	struct hi3660_mbox *mbox = p;
	struct hi3660_mbox_chan *mchan;
	struct mbox_chan *chan;
	unsigned int state, intr_bit, i;
	unsigned int status, imask, todo;
	u32 msg[MBOX_MSG_LEN];

	state = __ipc_mbox_istatus(mbox->base, 0);

	if (!state) {
		dev_warn(mbox->dev, "%s: spurious interrupt\n",
			 __func__);
		return IRQ_HANDLED;
	}

	while (state) {
		intr_bit = __ffs(state);
		state &= (state - 1);

		chan = mbox->irq_map_chan[intr_bit];
		if (!chan) {
			dev_warn(mbox->dev, "%s: unexpected irq vector %d\n",
				 __func__, intr_bit);
			continue;
		}

		mchan = chan->con_priv;

		for (i = 0; i < MBOX_MSG_LEN; i++)
			mchan->buf[i] = __ipc_read(mbox->base, mchan->slot, i);

		if (mchan->dir == MBOX_TX)
			mbox_chan_txdone(chan, 0);
		else
			mbox_chan_received_data(chan, (void *)msg);

		for (i = 0; i < MBOX_MSG_LEN; i++)
			__ipc_write(mbox->base, 0x0, mchan->slot, i);

		imask = __ipc_cpu_imask_get(mbox->base, mchan->slot);
	        todo = ((1<<0) | (1<<1)) & (~imask);
		__ipc_cpu_iclr(mbox->base, todo, mchan->slot);


		status = __ipc_status(mbox->base, mchan->slot);
		if ((MBOX_DESTINATION_STATUS & status) &&
		    (!(AUTOMATIC_ACK_CONFIG & status)))
			__ipc_send(mbox->base, todo, mchan->slot);

		/*release the channel */
		_mdev_release(mchan);
	}

	return IRQ_HANDLED;
}

static int hi3660_mbox_startup(struct mbox_chan *chan)
{
	struct hi3660_mbox_chan *mchan;

	mchan = chan->con_priv;

	if (mchan->irq_mode)
		chan->txdone_method = TXDONE_BY_IRQ;

	return 0;
}

static void hi3660_mbox_shutdown(struct mbox_chan *chan)
{
	return;
}

static struct mbox_chan_ops hi3660_mbox_ops = {
	.send_data    = hi3660_mbox_send_data,
	.startup      = hi3660_mbox_startup,
	.shutdown     = hi3660_mbox_shutdown,
	.last_tx_done = hi3660_mbox_last_tx_done,
};

static struct mbox_chan *hi3660_mbox_xlate(struct mbox_controller *controller,
					   const struct of_phandle_args *spec)
{
	struct hi3660_mbox *mbox = dev_get_drvdata(controller->dev);
	struct hi3660_mbox_chan *mchan;
	struct mbox_chan *chan;
	unsigned int i = spec->args[0];
	unsigned int dst_irq = spec->args[1];
	unsigned int ack_irq = spec->args[2];

	/* Bounds checking */
	if (i >= mbox->chan_num) {
		dev_err(mbox->dev, "Invalid channel idx %d\n", i);
		return ERR_PTR(-EINVAL);
	}

	/* Is requested channel free? */
	chan = &mbox->chan[i];
	if (mbox->irq_map_chan[i] == (void *)chan) {
		dev_err(mbox->dev, "Channel in use\n");
		return ERR_PTR(-EBUSY);
	}

	mchan = chan->con_priv;
	mchan->dst_irq = dst_irq;
	mchan->ack_irq = ack_irq;

	mbox->irq_map_chan[i] = (void *)chan;
	return chan;
}

static const struct of_device_id hi3660_mbox_of_match[] = {
	{ .compatible = "hisilicon,hi3660-mbox", },
	{},
};
MODULE_DEVICE_TABLE(of, hi3660_mbox_of_match);

static int hi3660_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hi3660_mbox *mbox;
	struct resource *res;
	int i, err;

	mbox = devm_kzalloc(dev, sizeof(*mbox), GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	mbox->dev = dev;
	mbox->chan_num = MBOX_CHAN_MAX;
	mbox->mchan = devm_kzalloc(dev,
		mbox->chan_num * sizeof(*mbox->mchan), GFP_KERNEL);
	if (!mbox->mchan)
		return -ENOMEM;

	mbox->chan = devm_kzalloc(dev,
		mbox->chan_num * sizeof(*mbox->chan), GFP_KERNEL);
	if (!mbox->chan)
		return -ENOMEM;

	mbox->irq = platform_get_irq(pdev, 0);
	if (mbox->irq < 0)
		return mbox->irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mbox->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mbox->base)) {
		dev_err(dev, "ioremap buffer failed\n");
		return PTR_ERR(mbox->base);
	}

	err = devm_request_irq(dev, mbox->irq, hi3660_mbox_interrupt, 0,
			dev_name(dev), mbox);
	if (err) {
		dev_err(dev, "Failed to register a mailbox IRQ handler: %d\n",
			err);
		return -ENODEV;
	}

	mbox->controller.dev = dev;
	mbox->controller.chans = &mbox->chan[0];
	mbox->controller.num_chans = mbox->chan_num;
	mbox->controller.ops = &hi3660_mbox_ops;
	mbox->controller.of_xlate = hi3660_mbox_xlate;
	mbox->controller.txdone_poll = true;
	mbox->controller.txpoll_period = 5;

	for (i = 0; i < mbox->chan_num; i++) {
		mbox->chan[i].con_priv = &mbox->mchan[i];
		mbox->irq_map_chan[i] = NULL;

		mbox->mchan[i].parent = mbox;
		mbox->mchan[i].slot   = i;

		if (i == 28)
			/* channel 28 is used for thermal with irq mode */
			mbox->mchan[i].irq_mode = 1;
		else
			/* other channels use automatic mode */
			mbox->mchan[i].irq_mode = 0;
	}

	err = mbox_controller_register(&mbox->controller);
	if (err) {
		dev_err(dev, "Failed to register mailbox %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, mbox);
	dev_info(dev, "Mailbox enabled\n");
	return 0;
}

static int hi3660_mbox_remove(struct platform_device *pdev)
{
	struct hi3660_mbox *mbox = platform_get_drvdata(pdev);

	mbox_controller_unregister(&mbox->controller);
	return 0;
}

static struct platform_driver hi3660_mbox_driver = {
	.driver = {
		.name = "hi3660-mbox",
		.owner = THIS_MODULE,
		.of_match_table = hi3660_mbox_of_match,
	},
	.probe  = hi3660_mbox_probe,
	.remove = hi3660_mbox_remove,
};

static int __init hi3660_mbox_init(void)
{
	return platform_driver_register(&hi3660_mbox_driver);
}
core_initcall(hi3660_mbox_init);

static void __exit hi3660_mbox_exit(void)
{
	platform_driver_unregister(&hi3660_mbox_driver);
}
module_exit(hi3660_mbox_exit);

MODULE_AUTHOR("Leo Yan <leo.yan@linaro.org>");
MODULE_DESCRIPTION("Hi3660 mailbox driver");
MODULE_LICENSE("GPL v2");

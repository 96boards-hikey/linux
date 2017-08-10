/*
 * linux/sound/soc/m8m/hisi_i2s.c - I2S IP driver
 *
 * Copyright (C) 2015 Linaro, Ltd
 * Author: Andy Green <andy.green@linaro.org>
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
 * This driver only deals with S2 interface (BT)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <linux/interrupt.h>
#include <linux/reset.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/reset-controller.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include "hisi-i2s.h"

struct hisi_i2s {
	struct device *dev;
	struct reset_control *rc;
	int clocks;
	struct regulator *regu_asp;
	struct pinctrl *pctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_idle;
	struct clk *asp_subsys_clk;
	struct snd_soc_dai_driver dai;
	void __iomem *base;
	void __iomem *base_syscon;
	phys_addr_t base_phys;
	struct snd_dmaengine_dai_dma_data dma_data[2];
	spinlock_t lock;
	int rate;
	int format;
	int bits;
	int channels;
	u32 master;
	u32 status;
};

static void hisi_bits(struct hisi_i2s *i2s, u32 ofs, u32 reset, u32 set)
{
	u32 val = readl(i2s->base + ofs) & ~reset;

	writel(val | set, i2s->base + ofs);
}

static void hisi_syscon_bits(struct hisi_i2s *i2s, u32 ofs, u32 reset, u32 set)
{
	u32 val = readl(i2s->base_syscon + ofs) & ~reset;

	writel(val | set, i2s->base_syscon + ofs);
}

static int _hisi_i2s_set_fmt(struct hisi_i2s *i2s,
			       struct snd_pcm_substream *substream)
{
	switch (i2s->format & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		i2s->master = false;
		hisi_syscon_bits(i2s, HI_ASP_CFG_R_CLK_SEL_REG, 0, HI_ASP_CFG_R_CLK_SEL_EN);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		i2s->master = true;
		hisi_syscon_bits(i2s, HI_ASP_CFG_R_CLK_SEL_REG, HI_ASP_CFG_R_CLK_SEL_EN,0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int hisi_i2s_startup(struct snd_pcm_substream *substream,
		     struct snd_soc_dai *cpu_dai)
{
	struct hisi_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	/* deassert reset on sio_bt*/
	hisi_syscon_bits(i2s, HI_ASP_CFG_R_RST_CTRLDIS_REG, 0,BIT(2)|BIT(6)|BIT(8)|BIT(16));

	/* enable clk before frequency division */
	hisi_syscon_bits(i2s, HI_ASP_CFG_R_GATE_EN_REG, 0,BIT(5)|BIT(6));

	/* enable frequency division */
	hisi_syscon_bits(i2s, HI_ASP_CFG_R_GATE_CLKDIV_EN_REG, 0,BIT(2)|BIT(5));

	/* select clk */
	hisi_syscon_bits(i2s, HI_ASP_CFG_R_CLK_SEL_REG, HI_ASP_MASK,HI_ASP_CFG_R_CLK_SEL);

	/* select clk_div */
	hisi_syscon_bits(i2s, HI_ASP_CFG_R_CLK1_DIV_REG, HI_ASP_MASK,HI_ASP_CFG_R_CLK1_DIV_SEL);
	hisi_syscon_bits(i2s, HI_ASP_CFG_R_CLK4_DIV_REG, HI_ASP_MASK,HI_ASP_CFG_R_CLK4_DIV_SEL);
	hisi_syscon_bits(i2s, HI_ASP_CFG_R_CLK6_DIV_REG, HI_ASP_MASK,HI_ASP_CFG_R_CLK6_DIV_SEL);

	/* sio config */
	hisi_bits(i2s, HI_ASP_SIO_MODE_REG, HI_ASP_MASK, 0x0);
	hisi_bits(i2s, HI_ASP_SIO_DATA_WIDTH_SET_REG, HI_ASP_MASK, 0x9);
	hisi_bits(i2s, HI_ASP_SIO_I2S_POS_MERGE_EN_REG, HI_ASP_MASK, 0x1);
	hisi_bits(i2s, HI_ASP_SIO_I2S_START_POS_REG, HI_ASP_MASK, 0x0);

	return 0;
}
void hisi_i2s_shutdown(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *cpu_dai)
{
	struct hisi_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	if (!IS_ERR_OR_NULL(i2s->asp_subsys_clk)) {
		clk_disable_unprepare(i2s->asp_subsys_clk);
	}

}

static void hisi_i2s_txctrl(struct snd_soc_dai *cpu_dai, int on)
{
	struct hisi_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	spin_lock(&i2s->lock);

	if (on) {
		/* enable SIO TX */
		hisi_bits(i2s, HI_ASP_SIO_CT_SET_REG, 0,
			HI_ASP_SIO_TX_ENABLE | HI_ASP_SIO_TX_DATA_MERGE | HI_ASP_SIO_TX_FIFO_THRESHOLD |
			HI_ASP_SIO_RX_ENABLE |HI_ASP_SIO_RX_DATA_MERGE | HI_ASP_SIO_RX_FIFO_THRESHOLD);
	} else
		/* disable SIO TX */
		hisi_bits(i2s, HI_ASP_SIO_CT_CLR_REG, 0, HI_ASP_SIO_TX_ENABLE |HI_ASP_SIO_RX_ENABLE);
	spin_unlock(&i2s->lock);
}

static void hisi_i2s_rxctrl(struct snd_soc_dai *cpu_dai, int on)
{
	struct hisi_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	spin_lock(&i2s->lock);
	if (on)
		/* enable SIO RX */
		hisi_bits(i2s, HI_ASP_SIO_CT_SET_REG, 0,
			HI_ASP_SIO_TX_ENABLE | HI_ASP_SIO_TX_DATA_MERGE | HI_ASP_SIO_TX_FIFO_THRESHOLD |
			HI_ASP_SIO_RX_ENABLE |HI_ASP_SIO_RX_DATA_MERGE | HI_ASP_SIO_RX_FIFO_THRESHOLD);
	else
		/* disable SIO RX */
		hisi_bits(i2s, HI_ASP_SIO_CT_CLR_REG,0, HI_ASP_SIO_TX_ENABLE |HI_ASP_SIO_RX_ENABLE);
	spin_unlock(&i2s->lock);
}

static int hisi_i2s_set_sysclk(struct snd_soc_dai *cpu_dai,
			     int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int hisi_i2s_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct hisi_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	i2s->format = fmt;
	i2s->master = (i2s->format & SND_SOC_DAIFMT_MASTER_MASK) ==
		      SND_SOC_DAIFMT_CBS_CFS;

	return 0;
}

static int hisi_i2s_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *cpu_dai)
{
	struct hisi_i2s *i2s = dev_get_drvdata(cpu_dai->dev);
        struct snd_dmaengine_dai_dma_data *dma_data;

        dma_data = snd_soc_dai_get_dma_data(cpu_dai, substream);

	_hisi_i2s_set_fmt(i2s, substream);

	dma_data->maxburst = 4;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->addr = i2s->base_phys + HI_ASP_SIO_I2S_DUAL_TX_CHN_REG;
	else
		dma_data->addr = i2s->base_phys + HI_ASP_SIO_I2S_DUAL_RX_CHN_REG;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		i2s->bits = 16;
		dma_data->addr_width = 4;
		break;

	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		i2s->bits = 32;
		dma_data->addr_width = 4;
		break;
	default:
		dev_err(cpu_dai->dev, "Bad format\n");
		return -EINVAL;
	}

	return 0;
}

static int hisi_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			  struct snd_soc_dai *cpu_dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			hisi_i2s_rxctrl(cpu_dai, 1);
		else
			hisi_i2s_txctrl(cpu_dai, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			hisi_i2s_rxctrl(cpu_dai, 0);
		else
			hisi_i2s_txctrl(cpu_dai, 0);
		break;
	default:
		dev_err(cpu_dai->dev, "uknown cmd\n");
		return -EINVAL;
	}

	return 0;
}

static int hisi_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct hisi_i2s *i2s = snd_soc_dai_get_drvdata(dai);

        snd_soc_dai_init_dma_data(dai,
                                  &i2s->dma_data[SNDRV_PCM_STREAM_PLAYBACK],
                                  &i2s->dma_data[SNDRV_PCM_STREAM_CAPTURE]);

        return 0;
}


static struct snd_soc_dai_ops hisi_i2s_dai_ops = {
	.trigger	= hisi_i2s_trigger,
	.hw_params	= hisi_i2s_hw_params,
	.set_fmt	= hisi_i2s_set_fmt,
	.set_sysclk	= hisi_i2s_set_sysclk,
	.startup	= hisi_i2s_startup,
	.shutdown	= hisi_i2s_shutdown,
};

struct snd_soc_dai_driver hisi_i2s_dai_init = {
	.name = "hisi_i2s",
	.probe		= hisi_i2s_dai_probe,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_U16_LE,
		.rates = SNDRV_PCM_RATE_48000,
	},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_U16_LE,
		.rates = SNDRV_PCM_RATE_48000,
	},
	.ops = &hisi_i2s_dai_ops,
};

static const struct snd_soc_component_driver hisi_i2s_i2s_comp = {
	.name = "hisi_i2s-i2s",
};

#include <sound/dmaengine_pcm.h>

static const struct snd_pcm_hardware snd_hisi_hardware = {
        .info                   = SNDRV_PCM_INFO_MMAP |
                                  SNDRV_PCM_INFO_MMAP_VALID |
                                  SNDRV_PCM_INFO_PAUSE |
                                  SNDRV_PCM_INFO_RESUME |
                                  SNDRV_PCM_INFO_INTERLEAVED |
                                  SNDRV_PCM_INFO_HALF_DUPLEX,
        .period_bytes_min       = 4096,
        .period_bytes_max       = 4096,
        .periods_min            = 4,
        .periods_max            = UINT_MAX,
        .buffer_bytes_max       = SIZE_MAX,
};

static const struct snd_dmaengine_pcm_config hisi_dmaengine_pcm_config = {
        .pcm_hardware = &snd_hisi_hardware,
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
        .prealloc_buffer_size = 64 * 1024,
};

static int hisi_i2s_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hisi_i2s *i2s;
	struct resource *res;
	int ret;

	i2s = devm_kzalloc(dev,sizeof(*i2s), GFP_KERNEL);
	if (!i2s)
		return -ENOMEM;

	i2s->dev = dev;
	spin_lock_init(&i2s->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		return ret;
	}
	i2s->base_phys = (phys_addr_t)res->start;

	i2s->dai = hisi_i2s_dai_init;
	dev_set_drvdata(&pdev->dev, i2s);

	i2s->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(i2s->base)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = PTR_ERR(i2s->base);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		ret = -ENODEV;
		return ret;
	}
	i2s->base_syscon = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(i2s->base_syscon)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = PTR_ERR(i2s->base_syscon);
		return ret;
	}

	/* i2s iomux config */
	i2s->pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(i2s->pctrl)) {
		dev_err(dev, "could not get pinctrl\n");
		ret = -EIO;
		return ret;
	}

	i2s->pin_default = pinctrl_lookup_state(i2s->pctrl, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(i2s->pin_default)) {
		dev_err(dev, "could not get default state (%li)\n" , PTR_ERR(i2s->pin_default));
		ret = -EIO;
		return ret;
	}

	if (pinctrl_select_state(i2s->pctrl, i2s->pin_default)) {
		dev_err(dev, "could not set pins to default state\n");
		ret = -EIO;
		return ret;
	}

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev,
					      &hisi_dmaengine_pcm_config,
					      0);
	if (ret)
		return ret;

	ret = snd_soc_register_component(&pdev->dev, &hisi_i2s_i2s_comp,
					 &i2s->dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register dai\n");
		return ret;;
	}
	dev_info(&pdev->dev, "Registered as %s\n", i2s->dai.name);

	return 0;
}

static int hisi_i2s_remove(struct platform_device *pdev)
{
	struct hisi_i2s *i2s = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	dev_set_drvdata(&pdev->dev, NULL);

	pinctrl_put(i2s->pctrl);

	return 0;
}

static const struct of_device_id hisi_i2s_dt_ids[] = {
	{ .compatible = "hisilicon,hisi-i2s" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, hisi_i2s_dt_ids);

static struct platform_driver hisi_i2s_driver = {
	.probe = hisi_i2s_probe,
	.remove = hisi_i2s_remove,
	.driver = {
		.name = "hisi_i2s",
		.owner = THIS_MODULE,
		.of_match_table = hisi_i2s_dt_ids,
	},
};

module_platform_driver(hisi_i2s_driver);

MODULE_DESCRIPTION("Hisilicon I2S driver");
MODULE_AUTHOR("Andy Green <andy.green@linaro.org>");
MODULE_LICENSE("GPL");

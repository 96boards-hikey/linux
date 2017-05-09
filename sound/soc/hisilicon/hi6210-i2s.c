/*
 * linux/sound/soc/m8m/hi6210_i2s.c - I2S IP driver
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

#include "hi6210-i2s.h"

struct hi6210_i2s {
	struct device *dev;
	struct reset_control *rc;
	struct clk *clk[8];
	int clocks;
	struct snd_soc_dai_driver dai;
	void __iomem *base;
	void __iomem *base_syscon;
	void __iomem *base_pmctrl;
	phys_addr_t base_phys;
	struct snd_dmaengine_dai_dma_data dma_data[2];
	int clk_rate;
	spinlock_t lock;
	int rate;
	int format;
	u8 bits;
	u8 channels;
	u8 id;
	u8 channel_length;
	u8 use;
	u32 master:1;
	u32 status:1;
};

#define SC_PERIPH_CLKEN1	0x210
#define SC_PERIPH_CLKDIS1	0x214

#define SC_PERIPH_CLKEN3	0x230
#define SC_PERIPH_CLKDIS3	0x234

#define SC_PERIPH_CLKEN12	0x270
#define SC_PERIPH_CLKDIS12	0x274

#define SC_PERIPH_RSTEN1	0x310
#define SC_PERIPH_RSTDIS1	0x314
#define SC_PERIPH_RSTSTAT1	0x318

#define SC_PERIPH_RSTEN2	0x320
#define SC_PERIPH_RSTDIS2	0x324
#define SC_PERIPH_RSTSTAT2	0x328

#define SOC_PMCTRL_BBPPLLALIAS	0x48

static void hi6210_bits(struct hi6210_i2s *i2s, u32 ofs, u32 reset, u32 set)
{
	u32 val = readl(i2s->base + ofs) & ~reset;

	writel(val | set, i2s->base + ofs);
}

static int _hi6210_i2s_set_fmt(struct hi6210_i2s *i2s,
			       struct snd_pcm_substream *substream)
{
	u32 u;

	hi6210_bits(i2s, HII2S_ST_DL_FIFO_TH_CFG,
		    (HII2S_ST_DL_FIFO_TH_CFG__ST_DL_R_AEMPTY_MASK <<
		     HII2S_ST_DL_FIFO_TH_CFG__ST_DL_R_AEMPTY_SHIFT) |
		    (HII2S_ST_DL_FIFO_TH_CFG__ST_DL_R_AFULL_MASK <<
		     HII2S_ST_DL_FIFO_TH_CFG__ST_DL_R_AFULL_SHIFT) |
		    (HII2S_ST_DL_FIFO_TH_CFG__ST_DL_L_AEMPTY_MASK <<
		     HII2S_ST_DL_FIFO_TH_CFG__ST_DL_L_AEMPTY_SHIFT) |
		    (HII2S_ST_DL_FIFO_TH_CFG__ST_DL_L_AFULL_MASK <<
		     HII2S_ST_DL_FIFO_TH_CFG__ST_DL_L_AFULL_SHIFT),
		    (16 << HII2S_ST_DL_FIFO_TH_CFG__ST_DL_R_AEMPTY_SHIFT) |
		    (30 << HII2S_ST_DL_FIFO_TH_CFG__ST_DL_R_AFULL_SHIFT) |
		    (16 << HII2S_ST_DL_FIFO_TH_CFG__ST_DL_L_AEMPTY_SHIFT) |
		    (30 << HII2S_ST_DL_FIFO_TH_CFG__ST_DL_L_AFULL_SHIFT));

	hi6210_bits(i2s, HII2S_IF_CLK_EN_CFG, 0,
		    BIT(19) | BIT(18) | BIT(17) |
		    HII2S_IF_CLK_EN_CFG__S2_IF_CLK_EN |
		    HII2S_IF_CLK_EN_CFG__S2_OL_MIXER_EN |
		    HII2S_IF_CLK_EN_CFG__S2_OL_SRC_EN |
		    HII2S_IF_CLK_EN_CFG__ST_DL_R_EN |
		    HII2S_IF_CLK_EN_CFG__ST_DL_L_EN);

	hi6210_bits(i2s, HII2S_DIG_FILTER_CLK_EN_CFG,
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACR_SDM_EN |
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACR_HBF2I_EN |
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACR_AGC_EN |
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACL_SDM_EN |
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACL_HBF2I_EN |
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACL_AGC_EN,
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACR_MIXER_EN |
		    HII2S_DIG_FILTER_CLK_EN_CFG__DACL_MIXER_EN
	);

	hi6210_bits(i2s, HII2S_DIG_FILTER_MODULE_CFG,
		    HII2S_DIG_FILTER_MODULE_CFG__DACR_MIXER_IN2_MUTE |
		    HII2S_DIG_FILTER_MODULE_CFG__DACL_MIXER_IN2_MUTE,
		    0
	);
	hi6210_bits(i2s, HII2S_MUX_TOP_MODULE_CFG,
		    HII2S_MUX_TOP_MODULE_CFG__S2_OL_MIXER_IN1_MUTE |
		    HII2S_MUX_TOP_MODULE_CFG__S2_OL_MIXER_IN2_MUTE |
		    HII2S_MUX_TOP_MODULE_CFG__VOICE_DLINK_MIXER_IN1_MUTE |
		    HII2S_MUX_TOP_MODULE_CFG__VOICE_DLINK_MIXER_IN2_MUTE,
		    0
	);

	switch (i2s->format & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		i2s->master = false;
		hi6210_bits(i2s, HII2S_I2S_CFG, 0, HII2S_I2S_CFG__S2_MST_SLV);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		i2s->master = true;
		hi6210_bits(i2s, HII2S_I2S_CFG, HII2S_I2S_CFG__S2_MST_SLV, 0);
		break;
	default:
		return -EINVAL;
	}

	switch (i2s->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		u = HII2S_FORMAT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		u = HII2S_FORMAT_LEFT_JUST;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		u = HII2S_FORMAT_RIGHT_JUST;
		break;
	default:
		return -EINVAL;
	}

	/* set the i2s format */
	hi6210_bits(i2s, HII2S_I2S_CFG,
		    (HII2S_I2S_CFG__S2_FUNC_MODE_MASK <<
		     HII2S_I2S_CFG__S2_FUNC_MODE_SHIFT),
		    u << HII2S_I2S_CFG__S2_FUNC_MODE_SHIFT);

	/* misc control */
	hi6210_bits(i2s, HII2S_CLK_SEL,
		    HII2S_CLK_SEL__I2S_BT_FM_SEL | /* BT gets the I2S */
		    HII2S_CLK_SEL__EXT_12_288MHZ_SEL, /* internal clock src */
		    0);

	return 0;
}

int hi6210_i2s_startup(struct snd_pcm_substream *substream,
		     struct snd_soc_dai *cpu_dai)
{
	struct hi6210_i2s *i2s = dev_get_drvdata(cpu_dai->dev);
	int ret, n;

	/* deassert reset on ABB */
	if (readl(i2s->base_syscon + SC_PERIPH_RSTSTAT2) & BIT(4))
		writel(BIT(4), i2s->base_syscon + SC_PERIPH_RSTDIS2);

	for (n = 0; n < i2s->clocks; n++) {
		ret = clk_prepare_enable(i2s->clk[n]);
		if (ret)
			return ret;
	}

	ret = clk_set_rate(i2s->clk[1], 49152000);
	if (ret) {
		dev_err(i2s->dev, "%s: setting 49.152MHz base rate failed %d\n",
			__func__, ret);
		return ret;
	}

	/* enable clock before frequency division */
	writel(BIT(9), i2s->base_syscon + SC_PERIPH_CLKEN12);

	/* enable codec working clock / == "codec bus clock" */
	writel(BIT(5), i2s->base_syscon + SC_PERIPH_CLKEN1);

	/* deassert reset on codec / interface clock / working clock */
	writel(BIT(5), i2s->base_syscon + SC_PERIPH_RSTEN1);
	writel(BIT(5), i2s->base_syscon + SC_PERIPH_RSTDIS1);

	/* not interested in i2s irqs */
	hi6210_bits(i2s, HII2S_CODEC_IRQ_MASK, 0, 0x3f);

	/* reset the stereo downlink fifo */
	hi6210_bits(i2s, HII2S_APB_AFIFO_CFG_1, 0, BIT(5) | BIT(4));
	hi6210_bits(i2s, HII2S_APB_AFIFO_CFG_1, BIT(5) | BIT(4), 0);

	hi6210_bits(i2s, HII2S_SW_RST_N,
		    (HII2S_SW_RST_N__ST_DL_WORDLEN_MASK <<
		     HII2S_SW_RST_N__ST_DL_WORDLEN_SHIFT),
		    (HII2S_BITS_16 << HII2S_SW_RST_N__ST_DL_WORDLEN_SHIFT)
	);

	hi6210_bits(i2s, HII2S_MISC_CFG,
		    HII2S_MISC_CFG__ST_DL_TEST_SEL | /* mux 11/12 = APB not i2s */
		    HII2S_MISC_CFG__S2_DOUT_RIGHT_SEL | /* BT R ch  0 = mixer op of DACR ch */
		    HII2S_MISC_CFG__S2_DOUT_TEST_SEL,
		    HII2S_MISC_CFG__S2_DOUT_RIGHT_SEL |
		    HII2S_MISC_CFG__S2_DOUT_TEST_SEL /* BT L ch = 1 = mux 7 = "mixer output of DACL */
	);

	/* disable the local i2s reset */
	hi6210_bits(i2s, HII2S_SW_RST_N, 0, HII2S_SW_RST_N__SW_RST_N);

	return 0;
}
void hi6210_i2s_shutdown(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *cpu_dai)
{
	struct hi6210_i2s *i2s = dev_get_drvdata(cpu_dai->dev);
	int n;

	for (n = 0; n < i2s->clocks; n++)
		clk_disable_unprepare(i2s->clk[n]);

	writel(BIT(5), i2s->base_syscon + SC_PERIPH_RSTEN1);
}

static void hi6210_i2s_txctrl(struct snd_soc_dai *cpu_dai, int on)
{
	struct hi6210_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	spin_lock(&i2s->lock);

	if (on) {
		/* enable S2 TX */
		hi6210_bits(i2s, HII2S_I2S_CFG, 0, HII2S_I2S_CFG__S2_IF_TX_EN);
	} else
		/* disable S2 TX */
		hi6210_bits(i2s, HII2S_I2S_CFG, HII2S_I2S_CFG__S2_IF_TX_EN, 0);

	spin_unlock(&i2s->lock);
}

static void hi6210_i2s_rxctrl(struct snd_soc_dai *cpu_dai, int on)
{
	struct hi6210_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	spin_lock(&i2s->lock);
	if (on)
		hi6210_bits(i2s, HII2S_I2S_CFG, 0, HII2S_I2S_CFG__S2_IF_RX_EN);
	else
		hi6210_bits(i2s, HII2S_I2S_CFG, HII2S_I2S_CFG__S2_IF_RX_EN, 0);

	spin_unlock(&i2s->lock);
}

static int hi6210_i2s_set_sysclk(struct snd_soc_dai *cpu_dai,
			     int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int hi6210_i2s_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct hi6210_i2s *i2s = dev_get_drvdata(cpu_dai->dev);

	i2s->format = fmt;
	i2s->master = (i2s->format & SND_SOC_DAIFMT_MASTER_MASK) ==
		      SND_SOC_DAIFMT_CBS_CFS;

	return 0;
}

static int hi6210_i2s_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *cpu_dai)
{
	struct hi6210_i2s *i2s = dev_get_drvdata(cpu_dai->dev);
	u32 u, signed_data = 0;
        struct snd_dmaengine_dai_dma_data *dma_data;

        dma_data = snd_soc_dai_get_dma_data(cpu_dai, substream);

	_hi6210_i2s_set_fmt(i2s, substream);

	dma_data->maxburst = 2;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->addr = i2s->base_phys + HII2S_ST_DL_CHANNEL;
	else
		dma_data->addr = i2s->base_phys + HII2S_STEREO_UPLINK_CHANNEL;

	i2s->channels = params_channels(params);
	if (i2s->channels == 1)
		hi6210_bits(i2s, HII2S_I2S_CFG, 0, HII2S_I2S_CFG__S2_FRAME_MODE);
	else
		hi6210_bits(i2s, HII2S_I2S_CFG, HII2S_I2S_CFG__S2_FRAME_MODE, 0);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U16_LE:
		signed_data = HII2S_I2S_CFG__S2_CODEC_DATA_FORMAT;
		/* fallthru */
	case SNDRV_PCM_FORMAT_S16_LE:
		i2s->bits = 16;
		dma_data->addr_width = 2;
		u = HII2S_BITS_16;
		break;

	case SNDRV_PCM_FORMAT_U24_LE:
		signed_data = HII2S_I2S_CFG__S2_CODEC_DATA_FORMAT;
		/* fallthru */
	case SNDRV_PCM_FORMAT_S24_LE:
		i2s->bits = 32;
		u = HII2S_BITS_24;
		dma_data->addr_width = 3;
		break;
	default:
		dev_err(cpu_dai->dev, "Bad format\n");
		return -EINVAL;
	}

	/* clear loopback, set signed type and word length */
	hi6210_bits(i2s, HII2S_I2S_CFG,
		    HII2S_I2S_CFG__S2_CODEC_DATA_FORMAT |
		    (HII2S_I2S_CFG__S2_CODEC_IO_WORDLENGTH_MASK <<
		     HII2S_I2S_CFG__S2_CODEC_IO_WORDLENGTH_SHIFT) |
		    (HII2S_I2S_CFG__S2_DIRECT_LOOP_MASK <<
		     HII2S_I2S_CFG__S2_DIRECT_LOOP_SHIFT),
		    signed_data |
		    (u << HII2S_I2S_CFG__S2_CODEC_IO_WORDLENGTH_SHIFT));

	i2s->channel_length = i2s->channels * i2s->bits;
	i2s->rate = params_rate(params);

	switch (i2s->rate) {
	case 8000:
		u = HII2S_FS_RATE_8KHZ;
		break;
	case 16000:
		u = HII2S_FS_RATE_16KHZ;
		break;
	case 32000:
		u = HII2S_FS_RATE_32KHZ;
		break;
	case 48000:
		u = HII2S_FS_RATE_48KHZ;
		break;
	case 96000:
		u = HII2S_FS_RATE_96KHZ;
		break;
	case 192000:
		u = HII2S_FS_RATE_192KHZ;
		break;
	};

	if (!i2s->rate || !i2s->channel_length) {
		dev_err(cpu_dai->dev, "channels/rate/bits on i2s bad\n");
		return -EINVAL;
	}

	if (!i2s->master)
		return 0;

	/* set DAC and related units to correct rate */
	hi6210_bits(i2s, HII2S_FS_CFG,
		    (HII2S_FS_CFG__FS_S2_MASK <<
		     HII2S_FS_CFG__FS_S2_SHIFT) |
		    (HII2S_FS_CFG__FS_DACLR_MASK <<
		     HII2S_FS_CFG__FS_DACLR_SHIFT) |
		    (HII2S_FS_CFG__FS_ST_DL_R_MASK <<
		     HII2S_FS_CFG__FS_ST_DL_R_SHIFT) |
		    (HII2S_FS_CFG__FS_ST_DL_L_MASK <<
		     HII2S_FS_CFG__FS_ST_DL_L_SHIFT),
		    (u << HII2S_FS_CFG__FS_S2_SHIFT) |
		    (u << HII2S_FS_CFG__FS_DACLR_SHIFT) |
		    (u << HII2S_FS_CFG__FS_ST_DL_R_SHIFT) |
		    (u << HII2S_FS_CFG__FS_ST_DL_L_SHIFT)
	);

	return 0;
}

static int hi6210_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			  struct snd_soc_dai *cpu_dai)
{
	pr_debug("%s\n", __func__);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			hi6210_i2s_rxctrl(cpu_dai, 1);
		else
			hi6210_i2s_txctrl(cpu_dai, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			hi6210_i2s_rxctrl(cpu_dai, 0);
		else
			hi6210_i2s_txctrl(cpu_dai, 0);
		break;
	default:
		dev_err(cpu_dai->dev, "uknown cmd\n");
		return -EINVAL;
	}
	return 0;
}

static int hi6210_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct hi6210_i2s *i2s = snd_soc_dai_get_drvdata(dai);

        snd_soc_dai_init_dma_data(dai,
                                  &i2s->dma_data[SNDRV_PCM_STREAM_PLAYBACK],
                                  &i2s->dma_data[SNDRV_PCM_STREAM_CAPTURE]);

        return 0;
}


static struct snd_soc_dai_ops hi6210_i2s_dai_ops = {
	.trigger	= hi6210_i2s_trigger,
	.hw_params	= hi6210_i2s_hw_params,
	.set_fmt	= hi6210_i2s_set_fmt,
	.set_sysclk	= hi6210_i2s_set_sysclk,
	.startup	= hi6210_i2s_startup,
	.shutdown	= hi6210_i2s_shutdown,
};

struct snd_soc_dai_driver hi6210_i2s_dai_init = {
	.name = "hi6210_i2s",
	.probe		= hi6210_i2s_dai_probe,
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
	.ops = &hi6210_i2s_dai_ops,
};

static const struct snd_soc_component_driver hi6210_i2s_i2s_comp = {
	.name = "hi6210_i2s-i2s",
};

#include <sound/dmaengine_pcm.h>

static const struct snd_pcm_hardware snd_hi6210_hardware = {
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

static const struct snd_dmaengine_pcm_config hi6210_dmaengine_pcm_config = {
        .pcm_hardware = &snd_hi6210_hardware,
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
        .prealloc_buffer_size = 64 * 1024,
};

static int hi6210_i2s_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hi6210_i2s *i2s;
	struct resource *res;
	int ret;

	i2s = kzalloc(sizeof(*i2s), GFP_KERNEL);
	if (!i2s)
		return -ENOMEM;

	i2s->dev = dev;
	spin_lock_init(&i2s->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto err2;
	}
	i2s->base_phys = (phys_addr_t)res->start;

	i2s->dai = hi6210_i2s_dai_init;
	dev_set_drvdata(&pdev->dev, i2s);

	i2s->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(i2s->base)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = PTR_ERR(i2s->base);
		goto err2;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		ret = -ENODEV;
		goto err2;
	}
	i2s->base_syscon = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(i2s->base_syscon)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = PTR_ERR(i2s->base_syscon);
		goto err2;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		ret = -ENODEV;
		goto err2;
	}
	i2s->base_pmctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR(i2s->base_pmctrl)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = PTR_ERR(i2s->base_pmctrl);
		goto err2;
	}

	do {
		i2s->clk[i2s->clocks] = of_clk_get(pdev->dev.of_node,
						   i2s->clocks);
		if (IS_ERR_OR_NULL(i2s->clk[i2s->clocks]))
			break;
		i2s->clocks++;
	} while (i2s->clocks < ARRAY_SIZE(i2s->clk));
	if (!i2s->clocks) {
		ret = PTR_ERR(i2s->clk[0]);
		dev_err(&pdev->dev, "Failed to get clock\n");
		goto err2;
	}

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev,
					      &hi6210_dmaengine_pcm_config,
					      0);
	if (ret)
		goto err3;

	ret = snd_soc_register_component(&pdev->dev, &hi6210_i2s_i2s_comp,
					 &i2s->dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register dai\n");
		goto err3;
	}

	dev_info(&pdev->dev, "Registered as %s\n", i2s->dai.name);

	return 0;

err3:
	while (--i2s->clocks)
		clk_put(i2s->clk[i2s->clocks]);

err2:
	kfree(i2s);

	return ret;
}

static int hi6210_i2s_remove(struct platform_device *pdev)
{
	struct hi6210_i2s *i2s = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	dev_set_drvdata(&pdev->dev, NULL);
	iounmap(i2s->base);

	while (--i2s->clocks)
		clk_put(i2s->clk[i2s->clocks]);

	kfree(i2s);

	return 0;
}

static const struct of_device_id hi6210_i2s_dt_ids[] = {
	{ .compatible = "hisilicon,hi6210-i2s" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, hi6210_i2s_dt_ids);

static struct platform_driver hi6210_i2s_driver = {
	.probe = hi6210_i2s_probe,
	.remove = hi6210_i2s_remove,
	.driver = {
		.name = "hi6210_i2s",
		.owner = THIS_MODULE,
		.of_match_table = hi6210_i2s_dt_ids,
	},
};

module_platform_driver(hi6210_i2s_driver);

MODULE_DESCRIPTION("Hisilicon HI6210 I2S driver");
MODULE_AUTHOR("Andy Green <andy.green@linaro.org>");
MODULE_LICENSE("GPL");

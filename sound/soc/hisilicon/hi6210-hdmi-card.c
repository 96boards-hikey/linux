/*
 * linux/sound/soc/hisilicon/hi6210-hdmi-card.c
 *
 * Copyright (C) 2015 Linaro, Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/soc.h>

static int hdmi_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret)
		return ret;

	/* set i2s system clock */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, 24576000, SND_SOC_CLOCK_IN);
	if (ret)
		return ret;

	return 0;
}

/* operations of sound device */
static struct snd_soc_ops hdmi_ops = {
	.hw_params = hdmi_hw_params,
};

static struct snd_soc_dai_link hi6210_hdmi_dai_link = {
	.name = "hi6210-hdmi-dai-link",  /* "codec name" */
	.stream_name = "hdmi", /* stream name */

	.cpu_dai_name ="f7118000.hi6210_i2s",
	.codec_name = "0.hi6210_hdmi_card",
	.be_id = 0,
	.ops = &hdmi_ops,
	.codec_dai_name = "hi6210_hdmi_dai",
	.platform_name = "f7118000.hi6210_i2s",
};

static struct snd_soc_card snd_soc_hi6210_hdmi = {
	.name = "hi6210-hdmi",
	.owner = THIS_MODULE,
	.dai_link = &hi6210_hdmi_dai_link,
	.num_links = 1,
};

static const struct snd_soc_dai_ops hi6210_hdmi_dai_ops = {
};

static struct snd_soc_dai_driver hi6210_hdmi_dai = {
	.name = "hi6210_hdmi_dai",
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &hi6210_hdmi_dai_ops,
};

static struct snd_soc_codec_driver hi6210_hdmi_codec;

static int hi6210_hdmi_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_hi6210_hdmi;
	int ret;

	ret = snd_soc_register_codec(&pdev->dev, &hi6210_hdmi_codec,
			&hi6210_hdmi_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_codec failed (%d)\n", ret);
		return ret;
	}
	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		snd_soc_unregister_codec(&pdev->dev);
		card->dev = NULL;
		return ret;
	}
	return 0;
}

static int hi6210_hdmi_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	snd_soc_unregister_codec(&pdev->dev);
	card->dev = NULL;
	return 0;
}

static const struct of_device_id hi6210_hdmi_dt_ids[] = {
	{ .compatible = "hisilicon,hi6210-hdmi-audio-card" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, hi6210_hdmi_dt_ids);

static struct platform_driver hi6210_hdmi_driver = {
	.driver = {
		.name = "hi6210-hdmi-audio",
		.owner = THIS_MODULE,
		.of_match_table = hi6210_hdmi_dt_ids,
	},
	.probe = hi6210_hdmi_probe,
	.remove = hi6210_hdmi_remove,
};

module_platform_driver(hi6210_hdmi_driver);

MODULE_AUTHOR("andy.green@linaro.org");
MODULE_DESCRIPTION("Hisilicon HDMI machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:hi6210-hdmi-audio");

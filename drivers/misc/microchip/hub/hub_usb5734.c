/*
 * otgid_gpio_hub.c
 *
 * Copyright (c) Hisilicon Tech. Co., Ltd. All rights reserved.
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
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/hisi/log/hisi_log.h>
#include <linux/hisi/usb/hisi_usb.h>
#include <linux/tifm.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/hisi/usb/hub/hisi_hub.h>
#define DEVICE_DRIVER_NAME "gpio_hub_for_usb5734"

#define GPIO_HUB_OTG_HOST 1
#define GPIO_HUB_OTG_DEVICE 0
#define GPIO_TYPEC_VBUS_POWER 1
#define GPIO_TYPEC_NO_POWER 0
#define GPIO_HUB_VBUS_POWER 1
#define GPIO_HUB_VBUS_NO_POWER 0
#define GPIO_HUB_HUB_VBUS_POWER 1

/* SOC_CRGPERIPH_PEREN1_UNION */
#define SOC_CRGPERIPH_PEREN1_ADDR(base)               ((base) + (0x010))

#define HISILOG_TAG GPIO_HUB
HISILOG_REGIST();

struct gpio_hub_info {
	struct platform_device *pdev;
	int otg_switch_gpio;
	int typec_vbus_gpio;
	int typec_vbus_enable_val;
	int hub_vbus_gpio;
};

static struct gpio_hub_info gpio_hub_driver_info = {
	.otg_switch_gpio = -1,
	.typec_vbus_gpio = -1,
	.typec_vbus_enable_val = -1,
	.hub_vbus_gpio = -1,
};

void gpio_hub_power_off(void)
{
	if (gpio_is_valid(gpio_hub_driver_info.hub_vbus_gpio)) {
		gpio_set_value(gpio_hub_driver_info.hub_vbus_gpio,
			       GPIO_HUB_VBUS_NO_POWER);
		hisilog_info("%s: gpio hub hub vbus no power set success",
			     __func__);
	} else {
		hisilog_err("%s: gpio hub hub vbus no power set err",
			    __func__);
	}
}

void gpio_hub_power_on(void)
{
	if (gpio_is_valid(gpio_hub_driver_info.hub_vbus_gpio))
		gpio_set_value(gpio_hub_driver_info.hub_vbus_gpio,
			       GPIO_HUB_VBUS_POWER);
	else
		hisilog_err("%s: gpio hub hub vbus set err", __func__);
}

void gpio_hub_switch_to_hub(void)
{
	int gpio = gpio_hub_driver_info.otg_switch_gpio;

	if (!gpio_is_valid(gpio)) {
		hisilog_err("%s: otg_switch_gpio is err\n", __func__);
		return;
	}

	if (gpio_get_value(gpio)) {
		hisilog_info("%s: already switch to hub\n", __func__);
		return;
	}

	gpio_direction_output(gpio, 1);
	hisilog_err("%s: switch to hub\n", __func__);
}
EXPORT_SYMBOL_GPL(gpio_hub_switch_to_hub);

void gpio_hub_switch_to_typec(void)
{
	int gpio = gpio_hub_driver_info.otg_switch_gpio;

	if (!gpio_is_valid(gpio)) {
		hisilog_err("%s: otg_switch_gpio is err\n", __func__);
		return;
	}

	if (!gpio_get_value(gpio)) {
		hisilog_info("%s: already switch to typec\n", __func__);
		return;
	}

	gpio_direction_output(gpio, 0);
	hisilog_err("%s: switch to typec\n", __func__);
}
EXPORT_SYMBOL_GPL(gpio_hub_switch_to_typec);

static void gpio_hub_change_typec_power(int gpio, int on)
{
	if (!gpio_is_valid(gpio)) {
		hisilog_err("%s: typec power gpio is err\n", __func__);
		return;
	}

	if (gpio_get_value(gpio) == on) {
		hisilog_info("%s: typec power no change\n", __func__);
		return;
	}

	gpio_direction_output(gpio, on);
	hisilog_info("%s: set typec vbus gpio to %d\n", __func__, on);
}

void gpio_hub_typec_power_on(void)
{
	struct gpio_hub_info *info = &gpio_hub_driver_info;

	gpio_hub_change_typec_power(info->typec_vbus_gpio,
				    info->typec_vbus_enable_val);
}
EXPORT_SYMBOL_GPL(gpio_hub_typec_power_on);

void gpio_hub_typec_power_off(void)
{
	struct gpio_hub_info *info = &gpio_hub_driver_info;

	gpio_hub_change_typec_power(info->typec_vbus_gpio,
				    !info->typec_vbus_enable_val);
}
EXPORT_SYMBOL_GPL(gpio_hub_typec_power_off);

static int gpio_hub_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *root = pdev->dev.of_node;
	struct gpio_hub_info *info = &gpio_hub_driver_info;

	hisilog_info("%s: step in\n", __func__);

	info->pdev = pdev;
	if (!pdev)
		return -EBUSY;

	info->hub_vbus_gpio = of_get_named_gpio(root, "hub_vdd33_en_gpio", 0);
	if (!gpio_is_valid(info->hub_vbus_gpio)) {
		hisilog_err("%s: hub_vbus_gpio is err\n", __func__);
		return info->hub_vbus_gpio;
	}
	ret = gpio_request(info->hub_vbus_gpio, "hub_vbus_int_gpio");
	if (ret) {
		hisilog_err("%s: request hub_vbus_gpio err\n", __func__);
		return ret;
	}

	info->typec_vbus_gpio = of_get_named_gpio(root,
		"typc_vbus_int_gpio,typec-gpios", 0);
	if (!gpio_is_valid(info->hub_vbus_gpio)) {
		hisilog_err("%s: typec_vbus_gpio is err\n", __func__);
		ret = info->typec_vbus_gpio;
		goto free_gpio1;
	}
	ret = gpio_request(info->typec_vbus_gpio, "typc_vbus_int_gpio");
	if (ret) {
		hisilog_err("%s: request typec_vbus_gpio err\n", __func__);
		goto free_gpio1;
	}

	ret = of_property_read_u32(root, "typc_vbus_enable_val",
				   &info->typec_vbus_enable_val);
	if (ret) {
		hisilog_err("%s: typc_vbus_enable_val can't get\n", __func__);
		goto free_gpio2;
	}
	info->typec_vbus_enable_val = !!info->typec_vbus_enable_val;

	/* only for v2 */
	info->otg_switch_gpio = of_get_named_gpio(root, "otg_gpio", 0);
	if (!gpio_is_valid(info->otg_switch_gpio)) {
		hisilog_info("%s: otg_switch_gpio is err\n", __func__);
		info->otg_switch_gpio = -1;
	}

	ret = gpio_direction_output(info->hub_vbus_gpio, GPIO_HUB_VBUS_POWER);
	if (ret) {
		hisilog_err("%s: power on hub vbus err\n", __func__);
		goto free_gpio2;
	}

	ret = gpio_direction_output(info->typec_vbus_gpio,
				    info->typec_vbus_enable_val);
	if (ret) {
		hisilog_err("%s: power on typec vbus err", __func__);
		goto free_gpio2;
	}

	return 0;

free_gpio2:
	gpio_free(info->typec_vbus_gpio);
	info->typec_vbus_gpio = -1;
free_gpio1:
	gpio_free(info->hub_vbus_gpio);
	info->hub_vbus_gpio = -1;

	return ret;
}

static int  gpio_hub_remove(struct platform_device *pdev)
{
	struct gpio_hub_info *info = &gpio_hub_driver_info;

	if (gpio_is_valid(info->otg_switch_gpio)) {
		gpio_free(info->otg_switch_gpio);
		info->otg_switch_gpio = -1;
	}

	if (gpio_is_valid(info->typec_vbus_gpio)) {
		gpio_free(info->typec_vbus_gpio);
		info->typec_vbus_gpio = -1;
	}

	if (gpio_is_valid(info->hub_vbus_gpio)) {
		gpio_free(info->hub_vbus_gpio);
		info->hub_vbus_gpio = -1;
	}
	return 0;
}

static const struct of_device_id id_table_for_gpio_hub[] = {
	{.compatible = "hisilicon,gpio_hubv1"},
	{.compatible = "hisilicon,gpio_hubv2"},
	{}
};

static struct platform_driver gpio_hub_driver = {
	.probe = gpio_hub_probe,
	.remove = gpio_hub_remove,
	.driver = {
		.name = DEVICE_DRIVER_NAME,
		.of_match_table = of_match_ptr(id_table_for_gpio_hub),

	},
};

static int __init gpio_hub_init(void)
{
	int ret = platform_driver_register(&gpio_hub_driver);

	hisilog_info("%s:gpio hub init status:%d\n", __func__, ret);
	return ret;
}

static void __exit gpio_hub_exit(void)
{
	platform_driver_unregister(&gpio_hub_driver);
}

module_init(gpio_hub_init);
module_exit(gpio_hub_exit);

MODULE_AUTHOR("wangbinghui<wangbinghui@hisilicon.com>");
MODULE_DESCRIPTION("HUB GPIO FOR OTG ID driver");
MODULE_LICENSE("GPL v2");

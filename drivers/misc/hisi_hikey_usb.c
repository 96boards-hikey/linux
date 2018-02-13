/*
 *hisi_hikey_usb.c
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
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#define DEVICE_DRIVER_NAME "hisi_hikey_usb"

#define GPIO_HUB_OTG_HOST 1
#define GPIO_HUB_OTG_DEVICE 0
#define GPIO_TYPEC_VBUS_POWER 1
#define GPIO_TYPEC_NO_POWER 0
#define GPIO_HUB_VBUS_POWER 1
#define GPIO_HUB_VBUS_NO_POWER 0
#define GPIO_HUB_HUB_VBUS_POWER 1

/* SOC_CRGPERIPH_PEREN1_UNION */
#define SOC_CRGPERIPH_PEREN1_ADDR(base)               ((base) + (0x010))


struct gpio_hub_info {
	struct platform_device *pdev;
	int otg_switch_gpio;
	int typec_vbus_gpio;
	int typec_vbus_enable_val;
	int hub_vbus_gpio;
	int hub_reset_en_gpio;
	struct regulator *hub_regu;
};

static struct gpio_hub_info gpio_hub_driver_info = {
	.otg_switch_gpio = -1,
	.typec_vbus_gpio = -1,
	.typec_vbus_enable_val = -1,
	.hub_vbus_gpio = -1,
	.hub_regu = NULL,
};

void gpio_hub_power_off(void)
{
	int gpio = gpio_hub_driver_info.hub_vbus_gpio;
	int ret;

	if (gpio_is_valid(gpio)) {
		gpio_set_value(gpio, GPIO_HUB_VBUS_NO_POWER);
		pr_info("%s: gpio hub hub vbus no power set success",
			     __func__);
	}

	if (gpio_hub_driver_info.hub_regu &&
			regulator_is_enabled(gpio_hub_driver_info.hub_regu)) {
		ret = regulator_disable(gpio_hub_driver_info.hub_regu);
		if (ret)
			pr_err("disable hub regulator failed");
	}
}

void gpio_hub_power_on(void)
{
	int gpio = gpio_hub_driver_info.hub_vbus_gpio;
	int ret;

	if (gpio_is_valid(gpio))
		gpio_set_value(gpio, GPIO_HUB_VBUS_POWER);

	if (gpio_hub_driver_info.hub_regu &&
			!regulator_is_enabled(gpio_hub_driver_info.hub_regu)) {
		pr_err("%s:regulator_enable\n", __func__);
		ret = regulator_enable(gpio_hub_driver_info.hub_regu);
		if (ret)
			pr_err("enable hub regulator failed");
	}
}

void gpio_hub_switch_to_hub(void)
{
	int gpio = gpio_hub_driver_info.otg_switch_gpio;

	if (!gpio_is_valid(gpio)) {
		pr_err("%s: otg_switch_gpio is err\n", __func__);
		return;
	}

	if (gpio_get_value(gpio)) {
		pr_info("%s: already switch to hub\n", __func__);
		return;
	}

	gpio_direction_output(gpio, 1);
	pr_info("%s: switch to hub\n", __func__);
}
EXPORT_SYMBOL_GPL(gpio_hub_switch_to_hub);

void gpio_hub_switch_to_typec(void)
{
	int gpio = gpio_hub_driver_info.otg_switch_gpio;

	if (!gpio_is_valid(gpio)) {
		pr_err("%s: otg_switch_gpio is err\n", __func__);
		return;
	}

	if (!gpio_get_value(gpio)) {
		pr_info("%s: already switch to typec\n", __func__);
		return;
	}

	gpio_direction_output(gpio, 0);
	pr_info("%s: switch to typec\n", __func__);
}
EXPORT_SYMBOL_GPL(gpio_hub_switch_to_typec);

static void gpio_hub_change_typec_power(int gpio, int on)
{
	if (!gpio_is_valid(gpio)) {
		pr_err("%s: typec power gpio is err\n", __func__);
		return;
	}

	if (gpio_get_value(gpio) == on) {
		pr_info("%s: typec power no change\n", __func__);
		return;
	}

	gpio_direction_output(gpio, on);
	pr_info("%s: set typec vbus gpio to %d\n", __func__, on);
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
	struct regulator *hub_regu;

	pr_info("%s: step in\n", __func__);

	info->pdev = pdev;
	if (!pdev)
		return -EBUSY;

	if (of_device_is_compatible(root, "hisilicon,kirin970_hikey_usbhub")) {
		hub_regu = devm_regulator_get(&pdev->dev, "hub-vdd");
		if (IS_ERR(hub_regu)) {
			dev_err(&pdev->dev, "get hub-vdd-supply failed\n");
			return PTR_ERR(hub_regu);
		}
		info->hub_regu = hub_regu;
		ret = regulator_set_voltage(info->hub_regu, 3300000, 3300000);
		if (ret)
			dev_err(&pdev->dev, "set hub-vdd-supply voltage failed\n");

		info->hub_reset_en_gpio = of_get_named_gpio(root, "hub_reset_en_gpio", 0);
		if (!gpio_is_valid(info->hub_reset_en_gpio)) {
			pr_err("%s: hub_reset_en_gpio is err\n", __func__);
			return info->hub_reset_en_gpio;
		}

		ret = gpio_request(info->hub_reset_en_gpio, "hub_reset_en_gpio");
		if (ret) {
			pr_err("%s: request hub_reset_en_gpio err\n", __func__);
			return ret;
		}
		ret = gpio_direction_output(info->hub_reset_en_gpio, 1);
		if (ret) {
			pr_err("%s: set hub_reset_en_gpio 1 err\n", __func__);
			goto free_gpio1;
		}
	} else {
		info->hub_vbus_gpio = of_get_named_gpio(root, "hub_vdd33_en_gpio", 0);
		if (!gpio_is_valid(info->hub_vbus_gpio)) {
			pr_err("%s: hub_vbus_gpio is err\n", __func__);
			return info->hub_vbus_gpio;
		}

		ret = gpio_request(info->hub_vbus_gpio, "hub_vbus_int_gpio");
		if (ret) {
			pr_err("%s: request hub_vbus_gpio err\n", __func__);
			return ret;
		}

		ret = gpio_direction_output(info->hub_vbus_gpio, GPIO_HUB_VBUS_POWER);
		if (ret) {
			pr_err("%s: power on hub vbus err\n", __func__);
			goto free_gpio1;
		}
	}

	info->typec_vbus_gpio = of_get_named_gpio(root,
		"typc_vbus_int_gpio,typec-gpios", 0);
	if (!gpio_is_valid(info->typec_vbus_gpio)) {
		pr_err("%s: typec_vbus_gpio is err\n", __func__);
		ret = info->typec_vbus_gpio;
		goto free_gpio1;
	}
	ret = gpio_request(info->typec_vbus_gpio, "typc_vbus_int_gpio");
	if (ret) {
		pr_err("%s: request typec_vbus_gpio err\n", __func__);
		goto free_gpio1;
	}

	ret = of_property_read_u32(root, "typc_vbus_enable_val",
				   &info->typec_vbus_enable_val);
	if (ret) {
		pr_err("%s: typc_vbus_enable_val can't get\n", __func__);
		goto free_gpio2;
	}

	info->typec_vbus_enable_val = !!info->typec_vbus_enable_val;

	ret = gpio_direction_output(info->typec_vbus_gpio,
				    info->typec_vbus_enable_val);
	if (ret) {
		pr_err("%s: power on typec vbus err", __func__);
		goto free_gpio2;
	}

	if (!of_device_is_compatible(root, "hisilicon,gpio_hubv1")) {
		info->otg_switch_gpio = of_get_named_gpio(root, "otg_gpio", 0);
		if (!gpio_is_valid(info->otg_switch_gpio)) {
			pr_info("%s: otg_switch_gpio is err\n", __func__);
			info->otg_switch_gpio = -1;
		}

		ret = gpio_request(info->otg_switch_gpio, "otg_switch_gpio");
		if (ret) {
			pr_err("%s: request typec_vbus_gpio err\n", __func__);
			goto free_gpio2;
		}
	}

	return 0;

free_gpio2:
	gpio_free(info->typec_vbus_gpio);
	info->typec_vbus_gpio = -1;
free_gpio1:
	if (gpio_is_valid(info->hub_vbus_gpio)) {
		gpio_free(info->hub_vbus_gpio);
		info->hub_vbus_gpio = -1;
	}

	if (gpio_is_valid(info->hub_reset_en_gpio)) {
		gpio_free(info->hub_reset_en_gpio);
		info->hub_reset_en_gpio= -1;
	}

	return ret;
}

static int gpio_hub_remove(struct platform_device *pdev)
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
	{.compatible = "hisilicon,kirin970_hikey_usbhub"},
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

	pr_info("%s:gpio hub init status:%d\n", __func__, ret);
	return ret;
}

static void __exit gpio_hub_exit(void)
{
	platform_driver_unregister(&gpio_hub_driver);
}

module_init(gpio_hub_init);
module_exit(gpio_hub_exit);

MODULE_AUTHOR("wangbinghui<wangbinghui@hisilicon.com>");
MODULE_DESCRIPTION("Driver Support for USB functionality of Hikey");
MODULE_LICENSE("GPL v2");

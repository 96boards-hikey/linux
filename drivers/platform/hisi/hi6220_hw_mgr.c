/*
 * drivers/platform/hisi/hi6220_hw_mgr.c
 *
 * HW manager that initializes HW configuration from DT overlay.
 *
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct gpio_desc *of_get_named_gpiod_flags(struct device_node *np,
		const char *list_name, int index, enum of_gpio_flags *flags);

struct gpio_req {
	struct delayed_work gpio_work;
	struct device *dev;
	const char *name;
	unsigned long flags;
	struct gpio_desc *gpiod;
};

static char *hw_dt_entry;
module_param_named(dt_entry, hw_dt_entry, charp, 0644);

static struct workqueue_struct *hw_mgr_wq;
static atomic_t hw_mgr_of = ATOMIC_INIT(0);

static int platform_hw_mgr_apply_overlay(struct device_node *onp)
{
	int ret;

	ret = of_overlay_create(onp);
	if (ret < 0) {
		pr_err("hw_mgr: fail to create overlay: %d\n", ret);
		of_node_put(onp);
		return ret;
	}
	pr_info("hw_mgr: %s overlay applied\n", onp->name);
	return 0;
}

static int platform_hw_set_gpio(struct device *dev, struct gpio_desc *gpiod,
                                unsigned long flags, const char *name)
{
	int ret;

	ret = gpiod_direction_output(gpiod,
				     flags & GPIOF_OUT_INIT_HIGH ? 1 : 0);
	if (ret) {
		pr_err("hw_mgr: %s gpio error %d setting output\n", name, ret);
		return ret;
	}
	gpiod_put(gpiod);
	pr_info("hw_mgr: %s gpio set to 0x%lx\n", name, flags);
	return ret;
}

static void hw_mgr_gpio_work_func(struct work_struct *wsp)
{
	struct gpio_req *r = container_of(wsp, struct gpio_req,
					  gpio_work.work);

	platform_hw_set_gpio(r->dev, r->gpiod, r->flags, r->name);
	kfree(r);
}

static int platform_hw_mgr_set_gpio(struct device *dev, struct device_node *np)
{
	unsigned long flags;
	struct gpio_req *gpioreq;
	struct gpio_desc *gpiod;
	u32 value = 0;
	u32 delay = 0;

	gpiod = of_get_named_gpiod_flags(np, "set-gpio", 0, NULL);
	if (IS_ERR(gpiod)) {
		pr_err("hw_mgr: error %ld in %s gpio\n", PTR_ERR(gpiod),
			np->name);
		return PTR_ERR(gpiod);
	}
	of_property_read_u32_index(np, "value", 0, &value);
	flags = (value != 0) ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;

	if (of_property_read_u32_index(np, "delay", 0, &delay) ||
	    !hw_mgr_wq || (delay == 0))
		return platform_hw_set_gpio(dev, gpiod, flags, np->name);

	gpioreq = kmalloc(sizeof(*gpioreq), GFP_KERNEL);
	if (!gpioreq) {
		pr_err("hw_mgr: fail to allocate request\n");
		return -ENOMEM;
	}
	gpioreq->dev = dev;
	gpioreq->gpiod = gpiod;
	gpioreq->flags = flags;
	gpioreq->name = np->name;
	INIT_DELAYED_WORK(&gpioreq->gpio_work, hw_mgr_gpio_work_func);
	queue_delayed_work(hw_mgr_wq, &gpioreq->gpio_work,
			   msecs_to_jiffies(delay));
	return 0;
}

static int platform_hw_mgr_apply_dt(struct device *dev, char *dt_entry)
{
	struct device_node *enp = dev->of_node;
	struct device_node *next;
	struct device_node *prev = NULL;
	int ret = 0;

	if (!enp) {
		pr_err("hw_mgr: no dt entry\n");
		return -ENODEV;
	}
	enp = of_get_child_by_name(enp, dt_entry);
	if (!enp) {
		pr_err("hw_mgr: dt entry %s not found\n", dt_entry);
		return -ENODEV;
	}
	pr_info("hw_mgr: apply %s dt entry\n", enp->name);
	while ((next = of_get_next_available_child(enp, prev)) != NULL) {
		if (strncmp(next->name, "overlay", 7) == 0) {
			if (atomic_read(&hw_mgr_of) == 0)
				platform_hw_mgr_apply_overlay(next);
		} else if (strncmp(next->name, "gpio", 4) == 0) {
			ret = platform_hw_mgr_set_gpio(dev, next);
			if (ret)
				break;
		}
		prev = next;
	}
	atomic_set(&hw_mgr_of, 1);
	return ret;
}

static int hw_cfg_mgr_probe(struct platform_device *pdev)
{
	if (!hw_mgr_wq) {
		hw_mgr_wq = create_singlethread_workqueue("hw_mgr_wq");
		if (!hw_mgr_wq)
			pr_err("hw_mgr: can not create workqueue\n");
	}
	return platform_hw_mgr_apply_dt(&pdev->dev, hw_dt_entry);
}

static const struct of_device_id hw_cfg_mgr_match[] = {
	{ .compatible = "hw_cfg_manager", },
	{}
};

static struct platform_driver hw_cfg_mgr_driver = {
	.probe	= hw_cfg_mgr_probe,
	.driver	= {
		.name	= "hw_cfg_manager",
		.of_match_table = of_match_ptr(hw_cfg_mgr_match),
	},
};

static int __init platform_hw_mgr_init(void)
{
	if (!hw_dt_entry)
		return 0;
	return platform_driver_register(&hw_cfg_mgr_driver);
}

postcore_initcall(platform_hw_mgr_init);

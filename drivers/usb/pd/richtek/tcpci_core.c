/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Richtek TypeC Port Control Interface Core Driver
 *
 * Author: TH <tsunghan_tsai@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/hisi/log/hisi_log.h>
#include <linux/hisi/usb/pd/richtek/tcpci.h>
#include <linux/hisi/usb/pd/richtek/tcpci_typec.h>
#include <linux/hisi/usb/pd/richtek/rt1711h.h>

#ifdef CONFIG_USB_POWER_DELIVERY
#include "pd_dpm_prv.h"
#endif /* CONFIG_USB_POWER_DELIVERY */

#define TCPC_CORE_VERSION		"1.1.1_G"

static ssize_t tcpc_show_property(struct device *dev,
				  struct device_attribute *attr, char *buf);
static ssize_t tcpc_store_property(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

#define TCPC_DEVICE_ATTR(_name, _mode)					\
{									\
	.attr = { .name = #_name, .mode = _mode },			\
	.show = tcpc_show_property,					\
	.store = tcpc_store_property,					\
}

static struct class *tcpc_class;
EXPORT_SYMBOL_GPL(tcpc_class);

static struct device_type tcpc_dev_type;

static struct device_attribute tcpc_device_attributes[] = {
	TCPC_DEVICE_ATTR(role_def, 0444),
	TCPC_DEVICE_ATTR(rp_lvl, 0444),
	TCPC_DEVICE_ATTR(pd_test, 0664),
	TCPC_DEVICE_ATTR(info, 0444),
	TCPC_DEVICE_ATTR(timer, 0664),
	TCPC_DEVICE_ATTR(caps_info, 0444),
	TCPC_DEVICE_ATTR(cc_orient_info, 0444),
	TCPC_DEVICE_ATTR(remote_rp_lvl, 0444),
};

enum {
	TCPC_DESC_ROLE_DEF = 0,
	TCPC_DESC_RP_LEVEL,
	TCPC_DESC_PD_TEST,
	TCPC_DESC_INFO,
	TCPC_DESC_TIMER,
	TCPC_DESC_CAP_INFO,
	TCPC_DESC_CC_ORIENT_INFO,
	TCPC_DESC_REMOTE_RP_LEVEL,
};

static struct attribute *__tcpc_attrs[ARRAY_SIZE(tcpc_device_attributes) + 1];
static struct attribute_group tcpc_attr_group = {
	.attrs = __tcpc_attrs,
};

static const struct attribute_group *tcpc_attr_groups[] = {
	&tcpc_attr_group,
	NULL,
};

static const char * const role_text[] = {
	"SNK Only",
	"SRC Only",
	"DRP",
	"Try.SRC",
	"Try.SNK",
};

static ssize_t tcpc_show_property(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct tcpc_device *tcpc = to_tcpc_device(dev);
	const ptrdiff_t offset = attr - tcpc_device_attributes;
	int i = 0;
	int vmin, vmax, ioper;
	u8 cc1, cc2;
	bool from_ic = true;
	char cc1_buf[32] = {0};
	char cc2_buf[32] = {0};

	switch (offset) {
	case TCPC_DESC_CC_ORIENT_INFO:
		snprintf(buf, 256, "%s\n", tcpc->typec_polarity ? "2" : "1");
		TCPC_DBG("%s typec_polarity=%s\n", __func__, buf);
		break;
	case TCPC_DESC_CAP_INFO:
		snprintf(buf + strlen(buf), 256, "%s = %d\n%s = %d\n",
			 "local_selected_cap",
			tcpc->pd_port.local_selected_cap,
			"remote_selected_cap",
			tcpc->pd_port.remote_selected_cap);

		snprintf(buf + strlen(buf), 256, "%s\n",
			 "local_src_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.local_src_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.local_src_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf + strlen(buf), 256, "%d %d %d\n",
				 vmin, vmax, ioper);
		}
		snprintf(buf + strlen(buf), 256, "%s\n",
			 "local_snk_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.local_snk_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.local_snk_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf + strlen(buf), 256, "%d %d %d\n",
				 vmin, vmax, ioper);
		}
		snprintf(buf + strlen(buf), 256, "%s\n",
			 "remote_src_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.remote_src_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.remote_src_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf + strlen(buf), 256, "%d %d %d\n",
				 vmin, vmax, ioper);
		}
		snprintf(buf + strlen(buf), 256, "%s\n",
			 "remote_snk_cap(vmin, vmax, ioper)");
		for (i = 0; i < tcpc->pd_port.remote_snk_cap.nr; i++) {
			pd_extract_pdo_power(
				tcpc->pd_port.remote_snk_cap.pdos[i],
				&vmin, &vmax, &ioper);
			snprintf(buf + strlen(buf), 256, "%d %d %d\n",
				 vmin, vmax, ioper);
		}
		break;
	case TCPC_DESC_ROLE_DEF:
		snprintf(buf, 256, "%s\n", role_text[tcpc->desc.role_def]);
		break;
	case TCPC_DESC_RP_LEVEL:
		if (tcpc->typec_local_rp_level == TYPEC_CC_RP_DFT)
			snprintf(buf, 256, "%s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_1_5)
			snprintf(buf, 256, "%s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_3_0)
			snprintf(buf, 256, "%s\n", "3.0");
		break;
	case TCPC_DESC_REMOTE_RP_LEVEL:
		tcpm_inquire_remote_cc(tcpc, &cc1, &cc2, from_ic);

		if (cc1 == TYPEC_CC_VOLT_OPEN)
			snprintf(cc1_buf, 256, "%s\n", "OPEN");
		else if (cc1 == TYPEC_CC_VOLT_RA)
			snprintf(cc1_buf, 256, "%s\n", "RA");
		else if (cc1 == TYPEC_CC_VOLT_RD)
			snprintf(cc1_buf, 256, "%s\n", "RD");
		else if (cc1 == TYPEC_CC_VOLT_SNK_DFT)
			snprintf(cc1_buf, 256, "%s\n", "Default");
		else if (cc1 == TYPEC_CC_VOLT_SNK_1_5)
			snprintf(cc1_buf, 256, "%s\n", "1.5");
		else if (cc1 == TYPEC_CC_VOLT_SNK_3_0)
			snprintf(cc1_buf, 256, "%s\n", "3.0");
		else if (cc1 == TYPEC_CC_DRP_TOGGLING)
			snprintf(cc1_buf, 256, "%s\n", "DRP");
		else
			snprintf(cc1_buf, 256, "%s\n", "NULL");

		if (cc2 == TYPEC_CC_VOLT_OPEN)
			snprintf(cc2_buf, 256, "%s\n", "OPEN");
		else if (cc2 == TYPEC_CC_VOLT_RA)
			snprintf(cc2_buf, 256, "%s\n", "RA");
		else if (cc2 == TYPEC_CC_VOLT_RD)
			snprintf(cc2_buf, 256, "%s\n", "RD");
		else if (cc2 == TYPEC_CC_VOLT_SNK_DFT)
			snprintf(cc2_buf, 256, "%s\n", "Default");
		else if (cc2 == TYPEC_CC_VOLT_SNK_1_5)
			snprintf(cc2_buf, 256, "%s\n", "1.5");
		else if (cc2 == TYPEC_CC_VOLT_SNK_3_0)
			snprintf(cc2_buf, 256, "%s\n", "3.0");
		else if (cc2 == TYPEC_CC_DRP_TOGGLING)
			snprintf(cc2_buf, 256, "%s\n", "DRP");
		else
			snprintf(cc1_buf, 256, "%s\n", "NULL");

		snprintf(buf, 256, " cc1 %s cc2 %s\n", cc1_buf, cc2_buf);

		break;
	case TCPC_DESC_PD_TEST:
		snprintf(buf,
			 256, "%s\n%s\n%s\n%s\n%s\n", "1: Power Role Swap Test",
				"2: Data Role Swap Test", "3: Vconn Swap Test",
				"4: soft reset", "5: hard reset");
		break;
	case TCPC_DESC_INFO:
		i += snprintf(buf + i,
			256, "|^|==( %s info )==|^|\n", tcpc->desc.name);
		i += snprintf(buf + i,
			256, "role = %s\n", role_text[tcpc->desc.role_def]);
		if (tcpc->typec_local_rp_level == TYPEC_CC_RP_DFT)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "Default");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_1_5)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "1.5");
		else if (tcpc->typec_local_rp_level == TYPEC_CC_RP_3_0)
			i += snprintf(buf + i, 256, "rplvl = %s\n", "3.0");
		break;
	default:
		break;
	}
	return strlen(buf);
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else {
			return -EINVAL;
		}
	}
	return 0;
}

static ssize_t tcpc_store_property(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct tcpc_device *tcpc = to_tcpc_device(dev);
	struct tcpm_power_cap cap;
	const ptrdiff_t offset = attr - tcpc_device_attributes;
	int ret;
	long int val;

	switch (offset) {
	case TCPC_DESC_ROLE_DEF:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}

		tcpm_typec_change_role(tcpc, val);
		break;
	case TCPC_DESC_TIMER:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		#ifdef CONFIG_USB_POWER_DELIVERY
		if (val > 0 && val <= PD_PE_TIMER_END_ID)
			pd_enable_timer(&tcpc->pd_port, val);
		else if (val > PD_PE_TIMER_END_ID && val < PD_TIMER_NR)
			tcpc_enable_timer(tcpc, val);
		#else
		if (val > 0 && val < PD_TIMER_NR)
			tcpc_enable_timer(tcpc, val);
		#endif /* CONFIG_USB_POWER_DELIVERY */
		break;
	#ifdef CONFIG_USB_POWER_DELIVERY
	case TCPC_DESC_PD_TEST:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get parameters fail\n");
			return -EINVAL;
		}
		switch (val) {
		case 1: /* Power Role Swap */
			tcpm_power_role_swap(tcpc);
			break;
		case 2: /* Data Role Swap */
			tcpm_data_role_swap(tcpc);
			break;
		case 3: /* Vconn Swap */
			tcpm_vconn_swap(tcpc);
			break;
		case 4: /* Software Reset */
			tcpm_soft_reset(tcpc);
			break;
		case 5: /* Hardware Reset */
			tcpm_hard_reset(tcpc);
			break;
		case 6:
			tcpm_get_source_cap(tcpc, &cap);
			break;
		case 7:
			tcpm_get_sink_cap(tcpc, &cap);
			break;
		default:
			break;
		}
		break;
	#endif /* CONFIG_USB_POWER_DELIVERY */
	default:
		break;
	}
	return count;
}

static int tcpc_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct tcpc_device *tcpc = dev_get_drvdata(dev);

	return strcmp(tcpc->desc.name, name) == 0;
}

struct tcpc_device *tcpc_dev_get_by_name(const char *name)
{
	struct device *dev = class_find_device(tcpc_class,
			NULL, (const void *)name, tcpc_match_device_by_name);
	return dev ? dev_get_drvdata(dev) : NULL;
}

static void tcpc_device_release(struct device *dev)
{
	struct tcpc_device *tcpc_dev = to_tcpc_device(dev);
	char buf[1024] = { 0 };

	pr_info("%s : %s device release\n", __func__, dev_name(dev));
	if (!tcpc_dev)
		snprintf(buf, sizeof(buf), "the tcpc device is NULL\n");
	/* Un-init pe thread */
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpci_event_deinit(tcpc_dev);
#endif /* CONFIG_USB_POWER_DELIVERY */
	/* Un-init timer thread */
	tcpci_timer_deinit(tcpc_dev);
	/* Un-init Mutex */
	/* Do initialization */
	devm_kfree(dev, tcpc_dev);
}

static int pd_dpm_wake_lock_call(struct notifier_block *dpm_nb,
				 unsigned long event, void *data)
{
	struct tcpc_device *tcpc = container_of(dpm_nb,
						struct tcpc_device, dpm_nb);

	switch (event) {
	case PD_WAKE_LOCK:
		__pm_stay_awake(&tcpc->attach_wake_lock);
		break;
	case PD_WAKE_UNLOCK:
		__pm_relax(&tcpc->attach_wake_lock);
		break;
	default:
		pr_info("%s unknown event (%lu)\n", __func__, event);
		break;
	}

	return NOTIFY_OK;
}

static void tcpc_init_work(struct work_struct *work);

struct tcpc_device *tcpc_device_register(struct device *parent,
					 struct tcpc_desc *tcpc_desc,
					 struct tcpc_ops *ops, void *drv_data)
{
	struct tcpc_device *tcpc;
	int ret = 0;

	pr_info("%s register tcpc device (%s)\n", __func__, tcpc_desc->name);
	tcpc = devm_kzalloc(parent, sizeof(*tcpc), GFP_KERNEL);
	if (!tcpc) {
		pr_err("%s : allocate tcpc memeory failed\n", __func__);
		return NULL;
	}

	tcpc->dev.class = tcpc_class;
	tcpc->dev.type = &tcpc_dev_type;
	tcpc->dev.parent = parent;
	tcpc->dev.release = tcpc_device_release;
	dev_set_drvdata(&tcpc->dev, tcpc);
	tcpc->drv_data = drv_data;
	dev_set_name(&tcpc->dev, tcpc_desc->name);
	tcpc->desc = *tcpc_desc;
	tcpc->ops = ops;
	tcpc->typec_local_rp_level = tcpc_desc->rp_lvl;

	ret = device_register(&tcpc->dev);
	if (ret) {
		kfree(tcpc);
		return ERR_PTR(ret);
	}

	srcu_init_notifier_head(&tcpc->evt_nh);
	INIT_DELAYED_WORK(&tcpc->init_work, tcpc_init_work);

	mutex_init(&tcpc->access_lock);
	mutex_init(&tcpc->typec_lock);
	mutex_init(&tcpc->timer_lock);
	sema_init(&tcpc->timer_enable_mask_lock, 1);
	sema_init(&tcpc->timer_tick_lock, 1);

	/* If system support "WAKE_LOCK_IDLE", */
	/* please use it instead of "WAKE_LOCK_SUSPEND" */
	wakeup_source_init(&tcpc->attach_wake_lock,
			   "tcpc_attach_wakelock");
	wakeup_source_init(&tcpc->dettach_temp_wake_lock,
			   "tcpc_detach_wakelock");

	tcpc->dpm_nb.notifier_call = pd_dpm_wake_lock_call;
	ret = register_pd_wake_unlock_notifier(&tcpc->dpm_nb);
	if (ret < 0) {
		hisilog_err("%s register_pd_wake_unlock_notifier failed\n",
			    __func__);
	} else {
		hisilog_info("%s register_pd_wake_unlock_notifier OK\n",
			     __func__);
	}

	tcpci_timer_init(tcpc);
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpci_event_init(tcpc);
	pd_core_init(tcpc);
#endif /* CONFIG_USB_POWER_DELIVERY */

	return tcpc;
}
EXPORT_SYMBOL(tcpc_device_register);

static int tcpc_device_irq_enable(struct tcpc_device *tcpc)
{
	int ret;

	TCPC_DBG("%s\n", __func__);

	if (!tcpc->ops->init) {
		pr_err("%s Please implment tcpc ops init function\n",
		       __func__);
		return -EINVAL;
	}

	ret = tcpci_init(tcpc, false);
	if (ret < 0) {
		pr_err("%s tcpc init fail\n", __func__);
		return ret;
	}

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_init(tcpc, tcpc->desc.role_def + 1);
	tcpci_unlock_typec(tcpc);

	if (ret < 0) {
		pr_err("%s : tcpc typec init fail\n", __func__);
		return ret;
	}

	pr_info("%s : tcpc irq enable OK!\n", __func__);
	return 0;
}

static int tcpc_dec_notifier_supply_num(struct tcpc_device *tcp_dev)
{
	if (tcp_dev->desc.notifier_supply_num == 0) {
		pr_info("%s already started\n", __func__);
		return 0;
	}

	tcp_dev->desc.notifier_supply_num--;
	pr_info("%s supply_num = %d\n", __func__,
		tcp_dev->desc.notifier_supply_num);

	if (tcp_dev->desc.notifier_supply_num == 0) {
		cancel_delayed_work(&tcp_dev->init_work);
		tcpc_device_irq_enable(tcp_dev);
	}

	return 0;
}

struct tcpc_device *notify_tcp_dev_ready(const char *name)
{
	struct tcpc_device *tcpc = tcpc_dev_get_by_name(name);

	if (!tcpc)
		return NULL;

	tcpc_dec_notifier_supply_num(tcpc);
	return tcpc;
}

static void tcpc_init_work(struct work_struct *work)
{
	struct tcpc_device *tcpc = container_of(
		work, struct tcpc_device, init_work.work);

	if (tcpc->desc.notifier_supply_num == 0)
		return;

	pr_info("%s force start\n", __func__);

	tcpc->desc.notifier_supply_num = 0;
	tcpc_device_irq_enable(tcpc);
}

int tcpc_schedule_init_work(struct tcpc_device *tcpc)
{
	if (tcpc->desc.notifier_supply_num == 0)
		return tcpc_device_irq_enable(tcpc);

	pr_info("%s wait %d num\n", __func__, tcpc->desc.notifier_supply_num);

	schedule_delayed_work(
		&tcpc->init_work, msecs_to_jiffies(30 * 1000));
	return 0;
}
EXPORT_SYMBOL(tcpc_schedule_init_work);

int register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
			      struct notifier_block *nb)
{
	int ret;

	ret = srcu_notifier_chain_register(&tcp_dev->evt_nh, nb);
	if (ret != 0)
		return ret;

	tcpc_dec_notifier_supply_num(tcp_dev);
	return ret;
}
EXPORT_SYMBOL(register_tcp_dev_notifier);

int unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&tcp_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_tcp_dev_notifier);

void tcpc_device_unregister(struct device *dev, struct tcpc_device *tcpc)
{
	if (!tcpc)
		return;

	tcpc_typec_deinit(tcpc);

	wakeup_source_trash(&tcpc->dettach_temp_wake_lock);
	wakeup_source_trash(&tcpc->attach_wake_lock);

	device_unregister(&tcpc->dev);
}
EXPORT_SYMBOL(tcpc_device_unregister);

void *tcpc_get_dev_data(struct tcpc_device *tcpc)
{
	return tcpc->drv_data;
}
EXPORT_SYMBOL(tcpc_get_dev_data);

void tcpci_lock_typec(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->typec_lock);
}
EXPORT_SYMBOL(tcpci_lock_typec);

void tcpci_unlock_typec(struct tcpc_device *tcpc)
{
	mutex_unlock(&tcpc->typec_lock);
}
EXPORT_SYMBOL(tcpci_unlock_typec);

static void tcpc_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = tcpc_attr_groups;
	for (i = 0; i < ARRAY_SIZE(tcpc_device_attributes); i++)
		__tcpc_attrs[i] = &tcpc_device_attributes[i].attr;
}

static int __init tcpc_class_init(void)
{
	pr_info("%s_%s\n", __func__, TCPC_CORE_VERSION);

	tcpc_class = class_create(THIS_MODULE, "hisi_pd");
	if (IS_ERR(tcpc_class)) {
		pr_info("Unable to create tcpc class; errno = %ld\n",
			PTR_ERR(tcpc_class));
		return PTR_ERR(tcpc_class);
	}
	tcpc_init_attrs(&tcpc_dev_type);
	tcpc_class->suspend = NULL;
	tcpc_class->resume = NULL;

	pr_info("TCPC class init OK\n");
	return 0;
}

static void __exit tcpc_class_exit(void)
{
	class_destroy(tcpc_class);
	pr_info("TCPC class un-init OK\n");
}

subsys_initcall(tcpc_class_init);
module_exit(tcpc_class_exit);

MODULE_DESCRIPTION("Richtek TypeC Port Control Core");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION(TCPC_CORE_VERSION);
MODULE_LICENSE("GPL");

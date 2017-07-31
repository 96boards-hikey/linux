/************************************************************
 *
 * Copyright (C), Hisilicon Tech. Co., Ltd.
 * FileName: hisi_pd.c
 * Author: Hisilicon       Version : 0.1      Date:  2016-5-9
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  Description:  .c file for power delivery core layer which is used to handle
 *                  pulic logic management for different chips and to
 *                  provide interfaces for exteranl modules.
 *  Version:
 *  Function List:
 *  History:
 *  <author>  <time>   <version >   <desc>
 ***********************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/hisi/log/hisi_log.h>
#include <linux/hisi/usb/hisi_pd_dev.h>
#include <linux/hisi/usb/hisi_usb.h>
#include <linux/hisi/usb/pd/richtek/tcpm.h>

struct pd_dpm_info *g_pd_di;
static bool g_pd_cc_orientation;
static struct class *typec_class;
static struct device *typec_dev;
static int pd_dpm_typec_state;

#ifndef HISILOG_TAG
#define HISILOG_TAG hisi_pd
HISILOG_REGIST();
#endif

static bool pd_dpm_get_cc_orientation(void)
{
	hisilog_info("%s cc_orientation =%d\n", __func__, g_pd_cc_orientation);
	return g_pd_cc_orientation;
}

static void pd_dpm_set_cc_orientation(bool cc_orientation)
{
	hisilog_info("%s cc_orientation =%d\n", __func__, cc_orientation);
	g_pd_cc_orientation = cc_orientation;
}

void pd_dpm_get_typec_state(int *typec_state)
{
	hisilog_info("%s = %d\n",
		     __func__, pd_dpm_typec_state);

	*typec_state = pd_dpm_typec_state;
}

static void pd_dpm_set_typec_state(int typec_state)
{
	hisilog_info("%s = %d\n",
		     __func__, typec_state);

	pd_dpm_typec_state = typec_state;
}

static ssize_t pd_dpm_cc_orientation_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n",
		pd_dpm_get_cc_orientation() ? "2" : "1");
}

static ssize_t pd_dpm_pd_state_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n",
		pd_dpm_get_pd_finish_flag() ? "0" : "1");
}

static DEVICE_ATTR(cc_orientation, 0444, pd_dpm_cc_orientation_show, NULL);
static DEVICE_ATTR(pd_state, 0444, pd_dpm_pd_state_show, NULL);

static struct attribute *pd_dpm_ctrl_attributes[] = {
	&dev_attr_cc_orientation.attr,
	&dev_attr_pd_state.attr,
	NULL,
};

static const struct attribute_group pd_dpm_attr_group = {
	.attrs = pd_dpm_ctrl_attributes,
};

int pd_dpm_wake_unlock_notifier_call(struct pd_dpm_info *di,
				     unsigned long event, void *data)
{
	return atomic_notifier_call_chain(&di->pd_wake_unlock_evt_nh,
		event, data);
}

int pd_dpm_vbus_notifier_call(struct pd_dpm_info *di, unsigned long event,
			      void *data)
{
	hisilog_err("%s: pd_dpm_vbus_notifier_call!!!,++++\n",
		    __func__);
	return atomic_notifier_call_chain(&di->pd_evt_nh, event, data);
}

bool pd_dpm_get_pd_finish_flag(void)
{
	if (g_pd_di)
		return g_pd_di->pd_finish_flag;
	else
		return false;
}

bool pd_dpm_get_pd_source_vbus(void)
{
	if (g_pd_di)
		return g_pd_di->pd_source_vbus;
	else
		return false;
}

void pd_dpm_report_pd_source_vbus(struct pd_dpm_info *di, void *data)
{
	struct pd_dpm_vbus_state *vbus_state = data;

	mutex_lock(&di->sink_vbus_lock);

	if (vbus_state->vbus_type & TCP_VBUS_CTRL_PD_DETECT)
		di->pd_finish_flag = true;

	if (vbus_state->mv == 0) {
		hisilog_info("%s : Disable\n", __func__);
		pd_dpm_vbus_notifier_call(g_pd_di, CHARGER_TYPE_NONE, data);
	} else {
		di->pd_source_vbus = true;
		hisilog_info("%s : Source %d mV, %d mA\n",
			     __func__, vbus_state->mv, vbus_state->ma);
		pd_dpm_vbus_notifier_call(g_pd_di, PLEASE_PROVIDE_POWER, data);
	}
	mutex_unlock(&di->sink_vbus_lock);
}

void pd_dpm_report_pd_sink_vbus(struct pd_dpm_info *di, void *data)
{
	bool skip = false;
	unsigned long event;
	struct pd_dpm_vbus_state *vbus_state = data;

	mutex_lock(&di->sink_vbus_lock);

	if (vbus_state->vbus_type & TCP_VBUS_CTRL_PD_DETECT)
		di->pd_finish_flag = true;

	if (di->pd_finish_flag)
		event = PD_DPM_VBUS_TYPE_PD;
	else if (di->bc12_finish_flag)
		skip = true;
	else
		event = PD_DPM_VBUS_TYPE_TYPEC;

	if (!skip) {
		vbus_state = data;

		if (vbus_state->mv == 0) {
			if (event == PD_DPM_VBUS_TYPE_PD) {
				hisilog_info("%s : Disable\n", __func__);
				pd_dpm_vbus_notifier_call(g_pd_di,
							  CHARGER_TYPE_NONE,
							  data);
			}
		} else {
			di->pd_source_vbus = false;
			hisilog_info("%s : Sink %d mV, %d mA\n",
				     __func__, vbus_state->mv, vbus_state->ma);
			pd_dpm_vbus_notifier_call(g_pd_di, event, data);
		}
	} else {
		hisilog_info("%s : skip\n", __func__);
	}

	mutex_unlock(&di->sink_vbus_lock);
}

int pd_dpm_report_bc12(struct notifier_block *usb_nb, unsigned long event,
		       void *data)
{
	struct pd_dpm_info *di = container_of(usb_nb,
		struct pd_dpm_info, usb_nb);

	if (event == CHARGER_TYPE_NONE && !di->pd_finish_flag) {
		di->bc12_finish_flag = false;
		hisilog_info("%s : PD_WAKE_UNLOCK\n",
			     __func__);
		pd_dpm_wake_unlock_notifier_call(g_pd_di, PD_WAKE_UNLOCK, NULL);
	}

	if (event == PLEASE_PROVIDE_POWER)
		return NOTIFY_OK;

	if (!di->pd_finish_flag) {
		hisilog_info("%s : event (%lu)\n", __func__, event);
		pd_dpm_vbus_notifier_call(di, event, data);
	} else {
		hisilog_info("%s : igrone\n", __func__);
	}

	return NOTIFY_OK;
}

int register_pd_wake_unlock_notifier(struct notifier_block *nb)
{
	int ret = 0;

	if (!nb)
		return -EINVAL;

	if (!g_pd_di)
		return ret;

	ret = atomic_notifier_chain_register(&g_pd_di->pd_wake_unlock_evt_nh,
					     nb);
	if (ret != 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL(register_pd_wake_unlock_notifier);

int unregister_pd_wake_unlock_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister
		(&g_pd_di->pd_wake_unlock_evt_nh, nb);
}
EXPORT_SYMBOL(unregister_pd_wake_unlock_notifier);

int register_pd_dpm_notifier(struct notifier_block *nb)
{
	int ret = 0;

	if (!nb)
		return -EINVAL;

	if (!g_pd_di)
		return ret;

	ret = atomic_notifier_chain_register(&g_pd_di->pd_evt_nh, nb);
	if (ret != 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL(register_pd_dpm_notifier);

int unregister_pd_dpm_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&g_pd_di->pd_evt_nh, nb);
}
EXPORT_SYMBOL(unregister_pd_dpm_notifier);

static inline void pd_dpm_report_device_attach(void)
{
	hisilog_info("%s \r\n", __func__);
	if (pd_dpm_get_pd_finish_flag()) {
		hisilog_info("%s, in pd process, report charger connect event\n",
			     __func__);
		hisi_usb_otg_event(CHARGER_CONNECT_EVENT);
	}
}

static inline void pd_dpm_report_host_attach(void)
{
	hisilog_info("%s \r\n", __func__);
}

static inline void pd_dpm_report_device_detach(void)
{
	hisilog_info("%s \r\n", __func__);
	if (pd_dpm_get_pd_finish_flag()) {
		hisilog_info("%s, in pd process, report charger connect event\n",
			     __func__);
		hisi_usb_otg_event(CHARGER_DISCONNECT_EVENT);
	}
	pd_dpm_vbus_notifier_call(g_pd_di, CHARGER_TYPE_NONE, NULL);
}

static inline void pd_dpm_report_host_detach(void)
{
	hisilog_info("%s \r\n", __func__);
}

static void pd_dpm_report_attach(int new_state)
{
	switch (new_state) {
	case PD_DPM_USB_TYPEC_DEVICE_ATTACHED:
		pd_dpm_report_device_attach();
		break;

	case PD_DPM_USB_TYPEC_HOST_ATTACHED:
		pd_dpm_report_host_attach();
		break;
	}
}

static void pd_dpm_report_detach(int last_state)
{
	switch (last_state) {
	case PD_DPM_USB_TYPEC_DEVICE_ATTACHED:
		pd_dpm_report_device_detach();
		break;

	case PD_DPM_USB_TYPEC_HOST_ATTACHED:
		pd_dpm_report_host_detach();
		break;
	}
}

static void pd_dpm_usb_update_state(
		struct work_struct *work)
{
	int new_ev, last_ev;
	struct pd_dpm_info *usb_cb_data =
		container_of(to_delayed_work(work),
			     struct pd_dpm_info,
				 usb_state_update_work);

	mutex_lock(&usb_cb_data->usb_lock);
	new_ev = usb_cb_data->pending_usb_event;
	mutex_unlock(&usb_cb_data->usb_lock);

	last_ev = usb_cb_data->last_usb_event;

	if (last_ev == new_ev)
		return;

	switch (new_ev) {
	case PD_DPM_USB_TYPEC_DETACHED:
		pd_dpm_report_detach(last_ev);
		break;

	case PD_DPM_USB_TYPEC_DEVICE_ATTACHED:
	case PD_DPM_USB_TYPEC_HOST_ATTACHED:
		if (last_ev != PD_DPM_USB_TYPEC_DETACHED)
			pd_dpm_report_detach(last_ev);
		pd_dpm_report_attach(new_ev);
		break;
	default:
		return;
	}

	usb_cb_data->last_usb_event = new_ev;
}

int pd_dpm_handle_pe_event(unsigned long event, void *data)
{
	bool attach_event = false;
	int usb_event = PD_DPM_USB_TYPEC_NONE;
	struct pd_dpm_typec_state *typec_state = NULL;

	hisilog_err("%s:!!!,event=%ld,+++\n",
		    __func__, event);

	switch (event) {
	case PD_DPM_PE_EVT_TYPEC_STATE:
		{
			typec_state = data;
			switch (typec_state->new_state) {
			case PD_DPM_TYPEC_ATTACHED_SNK:
				attach_event = true;
				usb_event = PD_DPM_USB_TYPEC_DEVICE_ATTACHED;
				break;

			case PD_DPM_TYPEC_ATTACHED_SRC:
				attach_event = true;
				usb_event = PD_DPM_USB_TYPEC_HOST_ATTACHED;
				break;

			case PD_DPM_TYPEC_UNATTACHED:
				mutex_lock(&g_pd_di->sink_vbus_lock);
				g_pd_di->pd_finish_flag = false;
				g_pd_di->bc12_finish_flag = false;
				g_pd_di->pd_source_vbus = false;
				mutex_unlock(&g_pd_di->sink_vbus_lock);
				usb_event = PD_DPM_USB_TYPEC_DETACHED;
				break;

			default:
				hisilog_info("%s can not detect typec state\r\n",
					     __func__);
				break;
			}
			pd_dpm_set_typec_state(usb_event);
		}
		break;

	case PD_DPM_PE_EVT_PD_STATE:
		{
			struct pd_dpm_pd_state *pd_state = data;

			switch (pd_state->connected) {
			case PD_CONNECT_PE_READY_SNK:
			case PD_CONNECT_PE_READY_SRC:
				break;
			}
		}
		break;

	case PD_DPM_PE_EVT_DIS_VBUS_CTRL:
		{
			if (!g_pd_di) {
				hisilog_err("%s: g_pd_di is null!!!,+++\n",
					    __func__);
				return -1;
			}

			if (g_pd_di->pd_finish_flag) {
				struct pd_dpm_vbus_state vbus_state;

				hisilog_info("%s : Disable VBUS Control\n",
					     __func__);
				vbus_state.mv = 0;
				vbus_state.ma = 0;

				pd_dpm_vbus_notifier_call(g_pd_di,
							  CHARGER_TYPE_NONE,
							  &vbus_state);
			}
		}
		break;

	case PD_DPM_PE_EVT_SINK_VBUS:
		{
			pd_dpm_report_pd_sink_vbus(g_pd_di, data);
		}
		break;

	case PD_DPM_PE_EVT_SOURCE_VBUS:
		{
			pd_dpm_report_pd_source_vbus(g_pd_di, data);
		}
		break;

	case PD_DPM_PE_EVT_DR_SWAP:
		{
			struct pd_dpm_swap_state *swap_state = data;

			if (swap_state->new_role == PD_ROLE_DFP)
				usb_event = PD_DPM_USB_TYPEC_HOST_ATTACHED;
			else
				usb_event = PD_DPM_USB_TYPEC_DEVICE_ATTACHED;
		}
		break;

	case PD_DPM_PE_EVT_PR_SWAP:
		break;

	default:
		hisilog_info("%s  unkonw event \r\n", __func__);
		break;
	};

	if (attach_event)
		pd_dpm_set_cc_orientation(typec_state->polarity);

	if (usb_event != PD_DPM_USB_TYPEC_NONE) {
		mutex_lock(&g_pd_di->usb_lock);
		if (g_pd_di->pending_usb_event != usb_event) {
			cancel_delayed_work(&g_pd_di->usb_state_update_work);
			g_pd_di->pending_usb_event = usb_event;
			queue_delayed_work(g_pd_di->usb_wq,
					   &g_pd_di->usb_state_update_work,
					   msecs_to_jiffies(0));
		} else {
			pr_info("Pending event is same --> ignore this event %d\n",
				usb_event);
		}
		mutex_unlock(&g_pd_di->usb_lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pd_dpm_handle_pe_event);

static int pd_dpm_parse_dt(struct pd_dpm_info *info,
			   struct device *dev)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;
	/* default name */
	if (of_property_read_string(np, "tcp_name",
				    &info->tcpc_name) < 0)
		info->tcpc_name = "type_c_port0";

	return 0;
}

static int pd_dpm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pd_dpm_info *di;

	di = devm_kzalloc(&pdev->dev, sizeof(*di), GFP_KERNEL);
	di->dev = &pdev->dev;
	hisilog_info("%s : +++++++++\n", __func__);
	g_pd_di = di;

	mutex_init(&di->sink_vbus_lock);

	ATOMIC_INIT_NOTIFIER_HEAD(&di->pd_evt_nh);
	ATOMIC_INIT_NOTIFIER_HEAD(&di->pd_wake_unlock_evt_nh);

	di->usb_nb.notifier_call = pd_dpm_report_bc12;
	ret = hisi_charger_type_notifier_register(&di->usb_nb);
	if (ret < 0)
		hisilog_err("hisi_charger_type_notifier_register failed\n");

	if (typec_class) {
		typec_dev = device_create(typec_class, NULL, 0, NULL, "typec");
		ret = sysfs_create_group(&typec_dev->kobj, &pd_dpm_attr_group);
		if (ret)
			hisilog_err("%s: typec sysfs group create error\n",
				    __func__);
	}

	hisilog_info("%s ++++\r\n\r\n", __func__);

	di->last_usb_event = PD_DPM_USB_TYPEC_NONE;
	di->pending_usb_event = PD_DPM_USB_TYPEC_NONE;

	mutex_init(&di->usb_lock);

	di->usb_wq = create_workqueue("pd_dpm_usb_wq");
	INIT_DELAYED_WORK(&di->usb_state_update_work,
			  pd_dpm_usb_update_state);
	platform_set_drvdata(pdev, di);

	pd_dpm_parse_dt(di, &pdev->dev);
	notify_tcp_dev_ready(di->tcpc_name);

	return ret;
}
EXPORT_SYMBOL_GPL(pd_dpm_probe);

static const struct of_device_id pd_dpm_callback_match_table[] = {
	{.compatible = "hisilicon,pd_dpm",},
	{},
};

static struct platform_driver pd_dpm_callback_driver = {
	.probe		= pd_dpm_probe,
	.remove		= NULL,
	.driver		= {
		.name	= "hisilicon,pd_dpm",
		.owner	= THIS_MODULE,
		.of_match_table = pd_dpm_callback_match_table,
	}
};

static int __init pd_dpm_init(void)
{
	hisilog_info("%s\n", __func__);
	/*adjust the original product*/
	typec_class = class_create(THIS_MODULE, "hisi_typec");
	if (IS_ERR(typec_class)) {
		hisilog_err("%s: cannot create class\n", __func__);
		return PTR_ERR(typec_class);
	}

	return platform_driver_register(&pd_dpm_callback_driver);
}

static void __exit pd_dpm_exit(void)
{
	platform_driver_unregister(&pd_dpm_callback_driver);
}

device_initcall(pd_dpm_init);
module_exit(pd_dpm_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("hisilicon pd dpm");
MODULE_AUTHOR("wangbinghui<wangbinghui@hisilicon.com>");

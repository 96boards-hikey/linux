/*
 * Copyright (C) 2016 Richtek Technology Corp.
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

#ifndef __LINUX_RT_TCPC_H
#define __LINUX_RT_TCPC_H

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/cpu.h>
#include <linux/delay.h>

#include <linux/sched.h>
#include <linux/hisi/usb/pd/richtek/tcpci_core.h>
#ifdef CONFIG_USB_POWER_DELIVERY
#include <linux/hisi/usb/pd/richtek/pd_core.h>
#endif /* CONFIG_USB_POWER_DELIVERY */

#define PE_STATE_FULL_NAME	0

/* provide to TCPC interface */
int tcpci_report_usb_port_changed(struct tcpc_device *tcpc);
int tcpc_typec_init(struct tcpc_device *tcpc, u8 typec_role);
void tcpc_typec_deinit(struct tcpc_device *tcpc);
int tcpc_dual_role_phy_init(struct tcpc_device *tcpc);

struct tcpc_device *tcpc_device_register(
		struct device *parent, struct tcpc_desc *tcpc_desc,
		struct tcpc_ops *ops, void *drv_data);
void tcpc_device_unregister(
			struct device *dev, struct tcpc_device *tcpc);

int tcpc_schedule_init_work(struct tcpc_device *tcpc);

void *tcpc_get_dev_data(struct tcpc_device *tcpc);
void tcpci_lock_typec(struct tcpc_device *tcpc);
void tcpci_unlock_typec(struct tcpc_device *tcpc);
int tcpci_alert(struct tcpc_device *tcpc);

void tcpci_vbus_level_init(
		struct tcpc_device *tcpc, u16 power_status);

static inline int tcpci_check_vbus_valid(struct tcpc_device *tcpc)
{
	return tcpc->vbus_level >= TCPC_VBUS_VALID;
}

static inline int tcpci_check_vsafe0v(struct tcpc_device *tcpc, bool detect_en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = (tcpc->vbus_level == TCPC_VBUS_SAFE0V);
#else
	ret = (tcpc->vbus_level == TCPC_VBUS_INVALID);
#endif

	return ret;
}

static inline int tcpci_alert_status_clear(
		struct tcpc_device *tcpc, u32 mask)
{
	return tcpc->ops->alert_status_clear(tcpc, mask);
}

static inline int tcpci_fault_status_clear(
	struct tcpc_device *tcpc, u8 status)
{
	if (tcpc->ops->fault_status_clear)
		return tcpc->ops->fault_status_clear(tcpc, status);
	return 0;
}

static inline int tcpci_get_alert_status(
		struct tcpc_device *tcpc, u32 *alert)
{
	return tcpc->ops->get_alert_status(tcpc, alert);
}

static inline int tcpci_get_fault_status(
		struct tcpc_device *tcpc, u8 *fault)
{
	if (tcpc->ops->get_fault_status)
		return tcpc->ops->get_fault_status(tcpc, fault);
	*fault = 0;
	return 0;
}

static inline int tcpci_get_power_status(
		struct tcpc_device *tcpc, u16 *pw_status)
{
	return tcpc->ops->get_power_status(tcpc, pw_status);
}

static inline int tcpci_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	u16 power_status;

	ret = tcpc->ops->init(tcpc, sw_reset);
	if (ret)
		return ret;

	ret = tcpci_get_power_status(tcpc, &power_status);
	if (ret)
		return ret;

	tcpci_vbus_level_init(tcpc, power_status);
	return 0;
}

static inline int tcpci_get_cc(struct tcpc_device *tcpc)
{
	int ret, cc1, cc2;

	ret = tcpc->ops->get_cc(tcpc, &cc1, &cc2);
	if (ret < 0)
		return ret;

	if ((cc1 == tcpc->typec_remote_cc[0]) &&
	    (cc2 == tcpc->typec_remote_cc[1])) {
		return 0;
	}

	tcpc->typec_remote_cc[0] = cc1;
	tcpc->typec_remote_cc[1] = cc2;
	return 1;
}

static inline int tcpci_set_cc(struct tcpc_device *tcpc, int pull)
{
#ifdef CONFIG_USB_PD_DBG_ALWAYS_LOCAL_RP
	if (pull == TYPEC_CC_RP)
		pull = tcpc->typec_local_rp_level;
#endif /* CONFIG_USB_PD_DBG_ALWAYS_LOCAL_RP */

	if (pull & TYPEC_CC_DRP) {
		tcpc->typec_remote_cc[0] =
		tcpc->typec_remote_cc[1] =
			TYPEC_CC_DRP_TOGGLING;
	}

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if ((pull == TYPEC_CC_DRP) && (tcpc->typec_legacy_cable)) {
		TCPC_INFO("LegacyCable-->\r\n");
		pull = TYPEC_CC_RP_1_5;
	}
#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	tcpc->typec_local_cc = pull;
	return tcpc->ops->set_cc(tcpc, pull);
}

static inline int tcpci_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	return tcpc->ops->set_polarity(tcpc, polarity);
}

static inline int tcpci_set_vconn(struct tcpc_device *tcpc, int enable)
{
	struct tcp_notify tcp_noti;

	tcp_noti.en_state.en = enable != 0;
	srcu_notifier_call_chain(&tcpc->evt_nh,
				 TCP_NOTIFY_SOURCE_VCONN, &tcp_noti);

	return tcpc->ops->set_vconn(tcpc, enable);
}

static inline int tcpci_set_low_power_mode(
	struct tcpc_device *tcpc, bool en, int pull)
{
	int rv = 0;

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	rv = tcpc->ops->set_low_power_mode(tcpc, en, pull);
#endif
	return rv;
}

#ifdef CONFIG_USB_POWER_DELIVERY

static inline int tcpci_set_msg_header(
	struct tcpc_device *tcpc, int power_role, int data_role)
{
	return tcpc->ops->set_msg_header(tcpc, power_role, data_role);
}

static inline int tcpci_set_rx_enable(struct tcpc_device *tcpc, u8 enable)
{
	return tcpc->ops->set_rx_enable(tcpc, enable);
}

static inline int tcpci_get_message(struct tcpc_device *tcpc,
				    u32 *payload, u16 *head,
					enum tcpm_transmit_type *type)
{
	return tcpc->ops->get_message(tcpc, payload, head, type);
}

static inline int tcpci_transmit(struct tcpc_device *tcpc,
				 enum tcpm_transmit_type type,
				 u16 header, const u32 *data)
{
	return tcpc->ops->transmit(tcpc, type, header, data);
}

static inline int tcpci_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	return tcpc->ops->set_bist_test_mode(tcpc, en);
}

static inline int tcpci_set_bist_carrier_mode(
		struct tcpc_device *tcpc, u8 pattern)
{
	if (pattern)	/* wait for GoodCRC */
		usleep_range(240, 260);

	return tcpc->ops->set_bist_carrier_mode(tcpc, pattern);
}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static inline int tcpci_retransmit(struct tcpc_device *tcpc)
{
	return tcpc->ops->retransmit(tcpc);
}
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#endif	/* CONFIG_USB_POWER_DELIVERY */

static inline int tcpci_notify_typec_state(
	struct tcpc_device *tcpc)
{
	struct pd_dpm_typec_state typec_state;

	typec_state.polarity = tcpc->typec_polarity;
	typec_state.old_state = tcpc->typec_attach_old;
	typec_state.new_state = tcpc->typec_attach_new;

	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_TYPEC_STATE, &typec_state);
	return 0;
}

static inline int tcpci_notify_role_swap(
	struct tcpc_device *tcpc, u8 event, u8 role)
{
#if 1
	u8 dpm_event;
	struct pd_dpm_swap_state swap_state;

	switch (event) {
	case TCP_NOTIFY_DR_SWAP:
		dpm_event = PD_DPM_PE_EVT_DR_SWAP;
		break;
	case TCP_NOTIFY_PR_SWAP:
		dpm_event = PD_DPM_PE_EVT_PR_SWAP;
		break;
	case TCP_NOTIFY_VCONN_SWAP:
		dpm_event = PD_DPM_PE_EVT_VCONN_SWAP;
		break;
	default:
		return 0;
	}

	swap_state.new_role = role;
	return pd_dpm_handle_pe_event(event, &swap_state);
#else
	return 0;
#endif
}

static inline int tcpci_notify_pd_state(
	struct tcpc_device *tcpc, u8 connect)
{
	struct pd_dpm_pd_state pd_state;

	pd_state.connected = connect;
	return pd_dpm_handle_pe_event(
		PD_DPM_PE_EVT_PD_STATE, &pd_state);
}

static inline int tcpci_disable_vbus_control(struct tcpc_device *tcpc)
{
	hisilog_err("%s: !!!++++++++\n", __func__);
#ifdef CONFIG_TYPEC_USE_DIS_VBUS_CTRL
	TCPC_DBG("disable_vbus\r\n");
	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_DIS_VBUS_CTRL, NULL);
	return 0;
#else
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_REMOVE, TCPC_VBUS_SINK_0V, 0);
	tcpci_source_vbus(tcpc, TCP_VBUS_CTRL_REMOVE, TCPC_VBUS_SOURCE_0V, 0);
	return 0;
#endif
	hisilog_err("%s: !!!-----------\n",
		    __func__);
}

static inline int tcpci_source_vbus(
	struct tcpc_device *tcpc, u8 type, int mv, int ma)
{
	struct pd_dpm_vbus_state vbus_state;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (type >= TCP_VBUS_CTRL_PD && tcpc->pd_port.pd_prev_connected)
		type |= TCP_VBUS_CTRL_PD_DETECT;
#endif

	if (ma < 0) {
		if (mv != 0) {
			switch (tcpc->typec_local_rp_level) {
			case TYPEC_CC_RP_1_5:
				ma = 1500;
				break;
			case TYPEC_CC_RP_3_0:
				ma = 3000;
				break;
			default:
			case TYPEC_CC_RP_DFT:
				ma = 500;
				break;
			}
		} else {
			ma = 0;
		}
	}

	vbus_state.ma = ma;
	vbus_state.mv = mv;
	vbus_state.vbus_type = type;

	TCPC_DBG("source_vbus: %d mV, %d mA\r\n", ma, mv);
	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_SOURCE_VBUS, &vbus_state);
	return 0;
}

static inline int tcpci_sink_vbus(
	struct tcpc_device *tcpc, u8 type, int mv, int ma)
{
	struct pd_dpm_vbus_state vbus_state;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (type >= TCP_VBUS_CTRL_PD && tcpc->pd_port.pd_prev_connected)
		type |= TCP_VBUS_CTRL_PD_DETECT;
#endif

	if (ma < 0) {
		if (mv != 0) {
			switch (tcpc->typec_remote_rp_level) {
			case TYPEC_CC_VOLT_SNK_1_5:
				ma = 1500;
				break;
			case TYPEC_CC_VOLT_SNK_3_0:
				ma = 3000;
				break;
			default:
			case TYPEC_CC_VOLT_SNK_DFT:
				ma = 500;
				break;
			}
		} else {
			ma = 0;
		}
	}

	vbus_state.ma = ma;
	vbus_state.mv = mv;
	vbus_state.vbus_type = type;

	TCPC_DBG("sink_vbus: %d mV, %d mA\r\n", ma, mv);
	pd_dpm_handle_pe_event(PD_DPM_PE_EVT_SINK_VBUS, &vbus_state);
	return 0;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static inline int tcpci_enter_mode(struct tcpc_device *tcpc,
				   u16 svid, u8 ops, u32 mode)
{
	/* DFP_U : DisplayPort Mode, USB Configuration */
	TCPC_INFO("EnterMode\r\n");
	return 0;
}

static inline int tcpci_exit_mode(
	struct tcpc_device *tcpc, u16 svid)
{
	TCPC_INFO("ExitMode\r\n");
	return 0;
}
#endif	/* CONFIG_USB_POWER_DELIVERY */

#endif /* #ifndef __LINUX_RT_TCPC_H */

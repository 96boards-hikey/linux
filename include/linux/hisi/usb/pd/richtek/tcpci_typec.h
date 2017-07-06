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

#ifndef __LINUX_TCPCI_TYPEC_H
#define __LINUX_TCPCI_TYPEC_H
#include <linux/hisi/usb/pd/richtek/tcpci.h>

struct tcpc_device;

/******************************************************************************
 *  Call following function to trigger TYPEC Connection State Change
 *
 * 1. H/W -> CC/PS Change.
 * 2. Timer -> CCDebounce or PDDebounce or others Timeout
 * 3. Policy Engine -> PR_SWAP, Error_Recovery, PE_Idle
 *****************************************************************************/

int tcpc_typec_handle_cc_change(
	struct tcpc_device *tcpc_dev);

int tcpc_typec_handle_ps_change(
		struct tcpc_device *tcpc_dev, int vbus_level);

int tcpc_typec_handle_timeout(
		struct tcpc_device *tcpc_dev, u32 timer_id);

int tcpc_typec_handle_vsafe0v(struct tcpc_device *tcpc_dev);

int tcpc_typec_set_rp_level(struct tcpc_device *tcpc_dev, u8 res);

int tcpc_typec_change_role(
	struct tcpc_device *tcpc_dev, u8 typec_role);

#ifdef CONFIG_USB_POWER_DELIVERY
int tcpc_typec_advertise_explicit_contract(struct tcpc_device *tcpc_dev);
int tcpc_typec_handle_pe_pr_swap(struct tcpc_device *tcpc_dev);
#else
int tcpc_typec_swap_role(struct tcpc_device *tcpc_dev);
#endif /* CONFIG_USB_POWER_DELIVERY */

#endif /* #ifndef __LINUX_TCPCI_TYPEC_H */

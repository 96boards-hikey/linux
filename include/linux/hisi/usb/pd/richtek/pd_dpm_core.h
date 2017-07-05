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

#ifndef PD_DPM_CORE_H
#define PD_DPM_CORE_H

#include <linux/hisi/usb/pd/richtek/tcpci.h>
#include <linux/hisi/usb/pd/richtek/pd_core.h>

/* ---- MISC ---- */
int pd_dpm_enable_vconn(pd_port_t *pd_port, bool en);
int pd_dpm_send_sink_caps(pd_port_t *pd_port);
int pd_dpm_send_source_caps(pd_port_t *pd_port);

/* ---- SNK ---- */

bool pd_dpm_send_request(pd_port_t *pd_port, int mv, int ma);

void pd_dpm_snk_evaluate_caps(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_snk_transition_power(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_snk_hard_reset(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- SRC ---- */

void pd_dpm_src_evaluate_request(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_src_transition_power(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_src_hard_reset(pd_port_t *pd_port);
void pd_dpm_src_inform_cable_vdo(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- UFP : Evaluate VDM Request ---- */

void pd_dpm_ufp_request_id_info(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_ufp_request_svid_info(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_ufp_request_mode_info(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_ufp_request_enter_mode(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_ufp_request_exit_mode(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- UFP : Response VDM Request ---- */

int pd_dpm_ufp_response_id(pd_port_t *pd_port, pd_event_t *pd_event);
int pd_dpm_ufp_response_svids(pd_port_t *pd_port, pd_event_t *pd_event);
int pd_dpm_ufp_response_modes(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- DFP : Inform VDM Result ---- */

void pd_dpm_dfp_inform_id(pd_port_t *pd_port, pd_event_t *pd_event, bool ack);
void pd_dpm_dfp_inform_svids(
			pd_port_t *pd_port, pd_event_t *pd_event, bool ack);
void pd_dpm_dfp_inform_modes(
			pd_port_t *pd_port, pd_event_t *pd_event, bool ack);
void pd_dpm_dfp_inform_enter_mode(
	pd_port_t *pd_port, pd_event_t *pd_event, bool ack);
void pd_dpm_dfp_inform_exit_mode(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_dfp_inform_attention(pd_port_t *pd_port, pd_event_t *pd_event);

void pd_dpm_dfp_inform_cable_vdo(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- DRP : Inform PowerCap ---- */

void pd_dpm_dr_inform_sink_cap(pd_port_t *pd_port, pd_event_t *pd_event);
void pd_dpm_dr_inform_source_cap(pd_port_t *pd_port, pd_event_t *pd_event);

/* ---- DRP : Data Role Swap ---- */

void pd_dpm_drs_evaluate_swap(pd_port_t *pd_port, uint8_t role);
void pd_dpm_drs_change_role(pd_port_t *pd_port, uint8_t role);

/* ---- DRP : Power Role Swap ---- */

void pd_dpm_prs_evaluate_swap(pd_port_t *pd_port, uint8_t role);
void pd_dpm_prs_turn_off_power_sink(pd_port_t *pd_port);
void pd_dpm_prs_enable_power_source(pd_port_t *pd_port, bool en);
void pd_dpm_prs_change_role(pd_port_t *pd_port, uint8_t role);

/* ---- DRP : Vconn Swap ---- */

void pd_dpm_vcs_evaluate_swap(pd_port_t *pd_port);
void pd_dpm_vcs_enable_vconn(pd_port_t *pd_port, bool en);

/* PE : Notify DPM */

int pd_dpm_notify_pe_startup(pd_port_t *pd_port);
int pd_dpm_notify_pe_hardreset(pd_port_t *pd_port);
int pd_dpm_notify_pe_ready(pd_port_t *pd_port, pd_event_t *pd_event);

/* TCPCI - VBUS Control */

static inline int pd_dpm_check_vbus_valid(pd_port_t *pd_port)
{
	return tcpci_check_vbus_valid(pd_port->tcpc_dev);
}

static inline int pd_dpm_sink_vbus(pd_port_t *pd_port, bool en)
{
	int mv = en ? TCPC_VBUS_SINK_5V : TCPC_VBUS_SINK_0V;

	return tcpci_sink_vbus(pd_port->tcpc_dev,
				TCP_VBUS_CTRL_REQUEST, mv, -1);
}

static inline int pd_dpm_source_vbus(pd_port_t *pd_port, bool en)
{
	int mv = en ? TCPC_VBUS_SOURCE_5V : TCPC_VBUS_SOURCE_0V;

	return tcpci_source_vbus(pd_port->tcpc_dev,
				TCP_VBUS_CTRL_REQUEST, mv, -1);
}
#endif /* PD_DPM_CORE_H */

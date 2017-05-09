/*
 * DesignWare MIPI DSI Host Controller v1.02 driver
 *
 * Copyright (c) 2016 Linaro Limited.
 * Copyright (c) 2014-2016 Hisilicon Limited.
 *
 * Author:
 *	<shizongxuan@huawei.com>
 *	<zhangxiubin@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_graph.h>
#include <linux/iopoll.h>
#include <video/mipi_display.h>
#include <linux/gpio/consumer.h>
#include <linux/of_address.h>

#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_encoder_slave.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_panel.h>

#include "dw_dsi_reg.h"
#include "kirin_dpe_reg.h"
#include "kirin_drm_dpe_utils.h"

#define DTS_COMP_DSI_NAME "hisilicon,hi3660-dsi"

#define ROUND(x, y)		((x) / (y) + \
				((x) % (y) * 10 / (y) >= 5 ? 1 : 0))
#define ROUND1(x, y)	((x) / (y) + ((x) % (y)  ? 1 : 0))
#define PHY_REF_CLK_RATE	19200000
#define PHY_REF_CLK_PERIOD_PS	(1000000000 / (PHY_REF_CLK_RATE / 1000))

#define encoder_to_dsi(encoder) \
	container_of(encoder, struct dw_dsi, encoder)
#define host_to_dsi(host) \
	container_of(host, struct dw_dsi, host)
#define connector_to_dsi(connector) \
	container_of(connector, struct dw_dsi, connector)
#define DSS_REDUCE(x)	((x) > 0 ? ((x) - 1) : (x))

enum dsi_output_client {
	OUT_HDMI = 0,
	OUT_PANEL,
	OUT_MAX
};

struct mipi_phy_params {
	u64 lane_byte_clk;
	u32 clk_division;

	u32 clk_lane_lp2hs_time;
	u32 clk_lane_hs2lp_time;
	u32 data_lane_lp2hs_time;
	u32 data_lane_hs2lp_time;
	u32 clk2data_delay;
	u32 data2clk_delay;

	u32 clk_pre_delay;
	u32 clk_post_delay;
	u32 clk_t_lpx;
	u32 clk_t_hs_prepare;
	u32 clk_t_hs_zero;
	u32 clk_t_hs_trial;
	u32 clk_t_wakeup;
	u32 data_pre_delay;
	u32 data_post_delay;
	u32 data_t_lpx;
	u32 data_t_hs_prepare;
	u32 data_t_hs_zero;
	u32 data_t_hs_trial;
	u32 data_t_ta_go;
	u32 data_t_ta_get;
	u32 data_t_wakeup;

	u32 phy_stop_wait_time;

	u32 rg_vrefsel_vcm;
	u32 rg_hstx_ckg_sel;
	u32 rg_pll_fbd_div5f;
	u32 rg_pll_fbd_div1f;
	u32 rg_pll_fbd_2p;
	u32 rg_pll_enbwt;
	u32 rg_pll_fbd_p;
	u32 rg_pll_fbd_s;
	u32 rg_pll_pre_div1p;
	u32 rg_pll_pre_p;
	u32 rg_pll_vco_750m;
	u32 rg_pll_lpf_rs;
	u32 rg_pll_lpf_cs;
	u32 rg_pll_enswc;
	u32 rg_pll_chp;

	u32 pll_register_override;		/*0x1E[0]*/
	u32 pll_power_down;			/*0x1E[1]*/
	u32 rg_band_sel;				/*0x1E[2]*/
	u32 rg_phase_gen_en;		/*0x1E[3]*/
	u32 reload_sel;				/*0x1E[4]*/
	u32 rg_pll_cp_p;				/*0x1E[7:5]*/
	u32 rg_pll_refsel;				/*0x16[1:0]*/
	u32 rg_pll_cp;				/*0x16[7:5]*/
	u32 load_command;
};

struct dsi_hw_ctx {
	void __iomem *base;
	char __iomem *peri_crg_base;

	struct clk *dss_dphy0_ref_clk;
	struct clk *dss_dphy1_ref_clk;
	struct clk *dss_dphy0_cfg_clk;
	struct clk *dss_dphy1_cfg_clk;
	struct clk *dss_pclk_dsi0_clk;
	struct clk *dss_pclk_dsi1_clk;
};

struct dw_dsi_client {
	u32 lanes;
	u32 phy_clock; /* in kHz */
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
};

struct mipi_panel_info {
	u8 dsi_version;
	u8 vc;
	u8 lane_nums;
	u8 lane_nums_select_support;
	u8 color_mode;
	u32 dsi_bit_clk; /* clock lane(p/n) */
	u32 burst_mode;
	u32 max_tx_esc_clk;
	u8 non_continue_en;

	u32 dsi_bit_clk_val1;
	u32 dsi_bit_clk_val2;
	u32 dsi_bit_clk_val3;
	u32 dsi_bit_clk_val4;
	u32 dsi_bit_clk_val5;
	u32 dsi_bit_clk_upt;
	/*uint32_t dsi_pclk_rate;*/

	u32 hs_wr_to_time;

	/* dphy config parameter adjust*/
	u32 clk_post_adjust;
	u32 clk_pre_adjust;
	u32 clk_pre_delay_adjust;
	u32 clk_t_hs_exit_adjust;
	u32 clk_t_hs_trial_adjust;
	u32 clk_t_hs_prepare_adjust;
	int clk_t_lpx_adjust;
	u32 clk_t_hs_zero_adjust;
	u32 data_post_delay_adjust;
	int data_t_lpx_adjust;
	u32 data_t_hs_prepare_adjust;
	u32 data_t_hs_zero_adjust;
	u32 data_t_hs_trial_adjust;
	u32 rg_vrefsel_vcm_adjust;

	/*only for Chicago<3660> use*/
	u32 rg_vrefsel_vcm_clk_adjust;
	u32 rg_vrefsel_vcm_data_adjust;
};

struct ldi_panel_info {
	u32 h_back_porch;
	u32 h_front_porch;
	u32 h_pulse_width;

	/*
	** note: vbp > 8 if used overlay compose,
	** also lcd vbp > 8 in lcd power on sequence
	*/
	u32 v_back_porch;
	u32 v_front_porch;
	u32 v_pulse_width;

	u8 hsync_plr;
	u8 vsync_plr;
	u8 pixelclk_plr;
	u8 data_en_plr;

	/* for cabc */
	u8 dpi0_overlap_size;
	u8 dpi1_overlap_size;
};

struct dw_dsi {
	struct drm_encoder encoder;
	struct drm_bridge *bridge;
	struct drm_panel *panel;
	struct mipi_dsi_host host;
	struct drm_connector connector; /* connector for panel */
	struct drm_display_mode cur_mode;
	struct dsi_hw_ctx *ctx;
	struct mipi_phy_params phy;
	struct mipi_panel_info mipi;
	struct ldi_panel_info ldi;
	u32 lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
	struct gpio_desc *gpio_mux;
	struct dw_dsi_client client[OUT_MAX];
	enum dsi_output_client cur_client;
	bool enable;
};

struct dsi_data {
	struct dw_dsi dsi;
	struct dsi_hw_ctx ctx;
};

struct dsi_phy_range {
	u32 min_range_kHz;
	u32 max_range_kHz;
	u32 pll_vco_750M;
	u32 hstx_ckg_sel;
};

static const struct dsi_phy_range dphy_range_info[] = {
	{   46875,    62500,   1,    7 },
	{   62500,    93750,   0,    7 },
	{   93750,   125000,   1,    6 },
	{  125000,   187500,   0,    6 },
	{  187500,   250000,   1,    5 },
	{  250000,   375000,   0,    5 },
	{  375000,   500000,   1,    4 },
	{  500000,   750000,   0,    4 },
	{  750000,  1000000,   1,    0 },
	{ 1000000,  1500000,   0,    0 }
};

void dsi_set_output_client(struct drm_device *dev)
{
	enum dsi_output_client client;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct dw_dsi *dsi;

	mutex_lock(&dev->mode_config.mutex);

	/* find dsi encoder */
	drm_for_each_encoder(encoder, dev)
		if (encoder->encoder_type == DRM_MODE_ENCODER_DSI)
			break;
	dsi = encoder_to_dsi(encoder);

	/* find HDMI connector */
	drm_for_each_connector(connector, dev)
		if (connector->connector_type == DRM_MODE_CONNECTOR_HDMIA)
			break;

	/*
	 * set the proper dsi output client
	 */
	client = connector->status == connector_status_connected ?
		OUT_HDMI : OUT_PANEL;
	if (client != dsi->cur_client) {
		/* associate bridge and dsi encoder */
		if (client == OUT_HDMI)
			encoder->bridge = dsi->bridge;
		else
			encoder->bridge = NULL;

		gpiod_set_value_cansleep(dsi->gpio_mux, client);
		dsi->cur_client = client;
		/* let the userspace know panel connector status has changed */
		drm_sysfs_hotplug_event(dev);
		DRM_INFO("client change to %s\n", client == OUT_HDMI ?
				 "HDMI" : "panel");
	}

	mutex_unlock(&dev->mode_config.mutex);
}
EXPORT_SYMBOL(dsi_set_output_client);

static void get_dsi_phy_ctrl(struct dw_dsi *dsi,
							struct mipi_phy_params *phy_ctrl)
{
	struct mipi_panel_info *mipi = NULL;
	struct drm_display_mode *mode = NULL;
	u32 dphy_req_kHz;
	int bpp;
	u32 id = 0;
	u32 ui = 0;
	u32 m_pll = 0;
	u32 n_pll = 0;
	u32 m_n_fract = 0;
	u32 m_n_int = 0;
	u64 lane_clock = 0;
	u64 vco_div = 1;

	u32 accuracy = 0;
	u32 unit_tx_byte_clk_hs = 0;
	u32 clk_post = 0;
	u32 clk_pre = 0;
	u32 clk_t_hs_exit = 0;
	u32 clk_pre_delay = 0;
	u32 clk_t_hs_prepare = 0;
	u32 clk_t_lpx = 0;
	u32 clk_t_hs_zero = 0;
	u32 clk_t_hs_trial = 0;
	u32 data_post_delay = 0;
	u32 data_t_hs_prepare = 0;
	u32 data_t_hs_zero = 0;
	u32 data_t_hs_trial = 0;
	u32 data_t_lpx = 0;
	u32 clk_pre_delay_reality = 0;
	u32 clk_t_hs_zero_reality = 0;
	u32 clk_post_delay_reality = 0;
	u32 data_t_hs_zero_reality = 0;
	u32 data_post_delay_reality = 0;
	u32 data_pre_delay_reality = 0;

	WARN_ON(!phy_ctrl);
	WARN_ON(!dsi);

	id = dsi->cur_client;
	mode = &dsi->cur_mode;
	mipi = &dsi->mipi;

	/*
	 * count phy params
	 */
	bpp = mipi_dsi_pixel_format_to_bpp(dsi->client[id].format);
	if (bpp < 0)
		return;
	if (mode->clock > 80000)
	    dsi->client[id].lanes = 4;
	else
	    dsi->client[id].lanes = 3;
	if (dsi->client[id].phy_clock)
		dphy_req_kHz = dsi->client[id].phy_clock;
	else
		dphy_req_kHz = mode->clock * bpp / dsi->client[id].lanes;

	lane_clock = dphy_req_kHz / 1000;
	DRM_INFO("Expected : lane_clock = %llu M\n", lane_clock);

	/************************  PLL parameters config  *********************/
	/*chip spec :
		If the output data rate is below 320 Mbps,
		RG_BNAD_SEL should be set to 1.
		At this mode a post divider of 1/4 will be applied to VCO.
	*/
	if ((320 <= lane_clock) && (lane_clock <= 2500)) {
		phy_ctrl->rg_band_sel = 0;	/*0x1E[2]*/
		vco_div = 1;
	} else if ((80 <= lane_clock) && (lane_clock < 320)) {
		phy_ctrl->rg_band_sel = 1;
		vco_div = 4;
	} else {
		DRM_ERROR("80M <= lane_clock< = 2500M, not support lane_clock = %llu M\n",
			lane_clock);
	}

	m_n_int = lane_clock * vco_div * 1000000UL / DEFAULT_MIPI_CLK_RATE;
	m_n_fract = ((lane_clock * vco_div * 1000000UL * 1000UL / DEFAULT_MIPI_CLK_RATE) % 1000) * 10 / 1000;

	if (m_n_int % 2 == 0) {
		if (m_n_fract * 6 >= 50) {
			n_pll = 2;
			m_pll = (m_n_int + 1) * n_pll;
		} else if (m_n_fract * 6 >= 30) {
			n_pll = 3;
			m_pll = m_n_int * n_pll + 2;
		} else {
			n_pll = 1;
			m_pll = m_n_int * n_pll;
		}
	} else {
		if (m_n_fract * 6 >= 50) {
			n_pll = 1;
			m_pll = (m_n_int + 1) * n_pll;
		} else if (m_n_fract * 6 >= 30) {
			n_pll = 1;
			m_pll = (m_n_int + 1) * n_pll;
		} else if (m_n_fract * 6 >= 10) {
			n_pll = 3;
			m_pll = m_n_int * n_pll + 1;
		} else {
			n_pll = 2;
			m_pll = m_n_int * n_pll;
		}
	}

	/*if set rg_pll_enswc=1, rg_pll_fbd_s can't be 0*/
	if (m_pll <= 8) {
		phy_ctrl->rg_pll_fbd_s = 1;
		phy_ctrl->rg_pll_enswc = 0;

		if (m_pll % 2 == 0) {
			phy_ctrl->rg_pll_fbd_p = m_pll / 2;
		} else {
			if (n_pll == 1) {
				n_pll *= 2;
				phy_ctrl->rg_pll_fbd_p = (m_pll  * 2) / 2;
			} else {
				DRM_ERROR("phy m_pll not support!m_pll = %d\n", m_pll);
				return;
			}
		}
	} else if (m_pll <= 300) {
		if (m_pll % 2 == 0)
			phy_ctrl->rg_pll_enswc = 0;
		else
			phy_ctrl->rg_pll_enswc = 1;

		phy_ctrl->rg_pll_fbd_s = 1;
		phy_ctrl->rg_pll_fbd_p = m_pll / 2;
	} else if (m_pll <= 315) {
		phy_ctrl->rg_pll_fbd_p = 150;
		phy_ctrl->rg_pll_fbd_s = m_pll - 2 * phy_ctrl->rg_pll_fbd_p;
		phy_ctrl->rg_pll_enswc = 1;
	} else {
		DRM_ERROR("phy m_pll not support!m_pll = %d\n", m_pll);
		return;
	}

	phy_ctrl->rg_pll_pre_p = n_pll;

	lane_clock = m_pll * (DEFAULT_MIPI_CLK_RATE / n_pll) / vco_div;
	DRM_INFO("Config : lane_clock = %llu\n", lane_clock);

	/*FIXME :*/
	phy_ctrl->rg_pll_cp = 1;		/*0x16[7:5]*/
	phy_ctrl->rg_pll_cp_p = 3;		/*0x1E[7:5]*/

	/*test_code_0x14 other parameters config*/
	phy_ctrl->rg_pll_enbwt = 0;	/*0x14[2]*/
	phy_ctrl->rg_pll_chp = 0;		/*0x14[1:0]*/

	/*test_code_0x16 other parameters config,  0x16[3:2] reserved*/
	phy_ctrl->rg_pll_lpf_cs = 0;	/*0x16[4]*/
	phy_ctrl->rg_pll_refsel = 1;		/*0x16[1:0]*/

	/*test_code_0x1E other parameters config*/
	phy_ctrl->reload_sel = 1;			/*0x1E[4]*/
	phy_ctrl->rg_phase_gen_en = 1;	/*0x1E[3]*/
	phy_ctrl->pll_power_down = 0;		/*0x1E[1]*/
	phy_ctrl->pll_register_override = 1;	/*0x1E[0]*/

	/*HSTX select VCM VREF*/
	phy_ctrl->rg_vrefsel_vcm = 0x55;
	if (mipi->rg_vrefsel_vcm_clk_adjust != 0)
		phy_ctrl->rg_vrefsel_vcm = (phy_ctrl->rg_vrefsel_vcm & 0x0F) |
			((mipi->rg_vrefsel_vcm_clk_adjust & 0x0F) << 4);

	if (mipi->rg_vrefsel_vcm_data_adjust != 0)
		phy_ctrl->rg_vrefsel_vcm = (phy_ctrl->rg_vrefsel_vcm & 0xF0) |
			(mipi->rg_vrefsel_vcm_data_adjust & 0x0F);

	/*if reload_sel = 1, need to set load_command*/
	phy_ctrl->load_command = 0x5A;

	/********************  clock/data lane parameters config  ******************/
	accuracy = 10;
	ui =  10 * 1000000000UL * accuracy / lane_clock;
	/*unit of measurement*/
	unit_tx_byte_clk_hs = 8 * ui;

	/* D-PHY Specification : 60ns + 52*UI <= clk_post*/
	clk_post = 600 * accuracy + 52 * ui + mipi->clk_post_adjust * ui;

	/* D-PHY Specification : clk_pre >= 8*UI*/
	clk_pre = 8 * ui + mipi->clk_pre_adjust * ui;

	/* D-PHY Specification : clk_t_hs_exit >= 100ns*/
	clk_t_hs_exit = 1000 * accuracy + mipi->clk_t_hs_exit_adjust * ui;

	/* clocked by TXBYTECLKHS*/
	clk_pre_delay = 0 + mipi->clk_pre_delay_adjust * ui;

	/* D-PHY Specification : clk_t_hs_trial >= 60ns*/
	/* clocked by TXBYTECLKHS*/
	clk_t_hs_trial = 600 * accuracy + 3 * unit_tx_byte_clk_hs + mipi->clk_t_hs_trial_adjust * ui;

	/* D-PHY Specification : 38ns <= clk_t_hs_prepare <= 95ns*/
	/* clocked by TXBYTECLKHS*/
	if (mipi->clk_t_hs_prepare_adjust == 0)
		mipi->clk_t_hs_prepare_adjust = 43;

	clk_t_hs_prepare = ((380 * accuracy + mipi->clk_t_hs_prepare_adjust * ui) <= (950 * accuracy - 8 * ui)) ?
		(380 * accuracy + mipi->clk_t_hs_prepare_adjust * ui) : (950 * accuracy - 8 * ui);

	/* clocked by TXBYTECLKHS*/
	data_post_delay = 0 + mipi->data_post_delay_adjust * ui;

	/* D-PHY Specification : data_t_hs_trial >= max( n*8*UI, 60ns + n*4*UI ), n = 1*/
	/* clocked by TXBYTECLKHS*/
	data_t_hs_trial = ((600 * accuracy + 4 * ui) >= (8 * ui) ? (600 * accuracy + 4 * ui) : (8 * ui)) + 8 * ui +
		3 * unit_tx_byte_clk_hs + mipi->data_t_hs_trial_adjust * ui;

	/* D-PHY Specification : 40ns + 4*UI <= data_t_hs_prepare <= 85ns + 6*UI*/
	/* clocked by TXBYTECLKHS*/
	if (mipi->data_t_hs_prepare_adjust == 0)
		mipi->data_t_hs_prepare_adjust = 35;

	data_t_hs_prepare = ((400  * accuracy + 4 * ui + mipi->data_t_hs_prepare_adjust * ui) <= (850 * accuracy + 6 * ui - 8 * ui)) ?
		(400  * accuracy + 4 * ui + mipi->data_t_hs_prepare_adjust * ui) : (850 * accuracy + 6 * ui - 8 * ui);

	/* D-PHY chip spec : clk_t_lpx + clk_t_hs_prepare > 200ns*/
	/* D-PHY Specification : clk_t_lpx >= 50ns*/
	/* clocked by TXBYTECLKHS*/
	clk_t_lpx = (((2000 * accuracy - clk_t_hs_prepare) >= 500 * accuracy) ?
		((2000 * accuracy - clk_t_hs_prepare)) : (500 * accuracy)) +
		mipi->clk_t_lpx_adjust * ui;

	/* D-PHY Specification : clk_t_hs_zero + clk_t_hs_prepare >= 300 ns*/
	/* clocked by TXBYTECLKHS*/
	clk_t_hs_zero = 3000 * accuracy - clk_t_hs_prepare + 3 * unit_tx_byte_clk_hs + mipi->clk_t_hs_zero_adjust * ui;

	/* D-PHY chip spec : data_t_lpx + data_t_hs_prepare > 200ns*/
	/* D-PHY Specification : data_t_lpx >= 50ns*/
	/* clocked by TXBYTECLKHS*/
	data_t_lpx = clk_t_lpx + mipi->data_t_lpx_adjust * ui; /*2000 * accuracy - data_t_hs_prepare;*/

	/* D-PHY Specification : data_t_hs_zero + data_t_hs_prepare >= 145ns + 10*UI*/
	/* clocked by TXBYTECLKHS*/
	data_t_hs_zero = 1450 * accuracy + 10 * ui - data_t_hs_prepare +
		3 * unit_tx_byte_clk_hs + mipi->data_t_hs_zero_adjust * ui;

	phy_ctrl->clk_pre_delay = ROUND1(clk_pre_delay, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_hs_prepare = ROUND1(clk_t_hs_prepare, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_lpx = ROUND1(clk_t_lpx, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_hs_zero = ROUND1(clk_t_hs_zero, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_hs_trial = ROUND1(clk_t_hs_trial, unit_tx_byte_clk_hs);

	phy_ctrl->data_post_delay = ROUND1(data_post_delay, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_hs_prepare = ROUND1(data_t_hs_prepare, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_lpx = ROUND1(data_t_lpx, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_hs_zero = ROUND1(data_t_hs_zero, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_hs_trial = ROUND1(data_t_hs_trial, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_ta_go = 4;
	phy_ctrl->data_t_ta_get = 5;

	clk_pre_delay_reality = phy_ctrl->clk_pre_delay + 2;
	clk_t_hs_zero_reality = phy_ctrl->clk_t_hs_zero + 8;
	data_t_hs_zero_reality = phy_ctrl->data_t_hs_zero + 4;
	data_post_delay_reality = phy_ctrl->data_post_delay + 4;

	phy_ctrl->clk_post_delay = phy_ctrl->data_t_hs_trial + ROUND1(clk_post, unit_tx_byte_clk_hs);
	phy_ctrl->data_pre_delay = clk_pre_delay_reality + phy_ctrl->clk_t_lpx +
		phy_ctrl->clk_t_hs_prepare + clk_t_hs_zero_reality + ROUND1(clk_pre, unit_tx_byte_clk_hs) ;

	clk_post_delay_reality = phy_ctrl->clk_post_delay + 4;
	data_pre_delay_reality = phy_ctrl->data_pre_delay + 2;

	phy_ctrl->clk_lane_lp2hs_time = clk_pre_delay_reality + phy_ctrl->clk_t_lpx +
		phy_ctrl->clk_t_hs_prepare + clk_t_hs_zero_reality + 3;
	phy_ctrl->clk_lane_hs2lp_time = clk_post_delay_reality + phy_ctrl->clk_t_hs_trial + 3;
	phy_ctrl->data_lane_lp2hs_time = data_pre_delay_reality + phy_ctrl->data_t_lpx +
		phy_ctrl->data_t_hs_prepare + data_t_hs_zero_reality + 3;
	phy_ctrl->data_lane_hs2lp_time = data_post_delay_reality + phy_ctrl->data_t_hs_trial + 3;
	phy_ctrl->phy_stop_wait_time = clk_post_delay_reality +
		phy_ctrl->clk_t_hs_trial + ROUND1(clk_t_hs_exit, unit_tx_byte_clk_hs) -
		(data_post_delay_reality + phy_ctrl->data_t_hs_trial) + 3;

	phy_ctrl->lane_byte_clk = lane_clock / 8;
	phy_ctrl->clk_division = (((phy_ctrl->lane_byte_clk / 2) % mipi->max_tx_esc_clk) > 0) ?
		(phy_ctrl->lane_byte_clk / 2 / mipi->max_tx_esc_clk + 1) :
		(phy_ctrl->lane_byte_clk / 2 / mipi->max_tx_esc_clk);

	DRM_INFO("PHY clock_lane and data_lane config : \n"
		"rg_vrefsel_vcm=%u\n"
		"clk_pre_delay=%u\n"
		"clk_post_delay=%u\n"
		"clk_t_hs_prepare=%u\n"
		"clk_t_lpx=%u\n"
		"clk_t_hs_zero=%u\n"
		"clk_t_hs_trial=%u\n"
		"data_pre_delay=%u\n"
		"data_post_delay=%u\n"
		"data_t_hs_prepare=%u\n"
		"data_t_lpx=%u\n"
		"data_t_hs_zero=%u\n"
		"data_t_hs_trial=%u\n"
		"data_t_ta_go=%u\n"
		"data_t_ta_get=%u\n",
		phy_ctrl->rg_vrefsel_vcm,
		phy_ctrl->clk_pre_delay,
		phy_ctrl->clk_post_delay,
		phy_ctrl->clk_t_hs_prepare,
		phy_ctrl->clk_t_lpx,
		phy_ctrl->clk_t_hs_zero,
		phy_ctrl->clk_t_hs_trial,
		phy_ctrl->data_pre_delay,
		phy_ctrl->data_post_delay,
		phy_ctrl->data_t_hs_prepare,
		phy_ctrl->data_t_lpx,
		phy_ctrl->data_t_hs_zero,
		phy_ctrl->data_t_hs_trial,
		phy_ctrl->data_t_ta_go,
		phy_ctrl->data_t_ta_get);
	DRM_INFO("clk_lane_lp2hs_time=%u\n"
		"clk_lane_hs2lp_time=%u\n"
		"data_lane_lp2hs_time=%u\n"
		"data_lane_hs2lp_time=%u\n"
		"phy_stop_wait_time=%u\n",
		phy_ctrl->clk_lane_lp2hs_time,
		phy_ctrl->clk_lane_hs2lp_time,
		phy_ctrl->data_lane_lp2hs_time,
		phy_ctrl->data_lane_hs2lp_time,
		phy_ctrl->phy_stop_wait_time);
}

static void dw_dsi_set_mode(struct dw_dsi *dsi, enum dsi_work_mode mode)
{
	struct dsi_hw_ctx *ctx = dsi->ctx;
	void __iomem *base = ctx->base;

	writel(RESET, base + PWR_UP);
	writel(mode, base + MODE_CFG);
	writel(POWERUP, base + PWR_UP);
}

static void dsi_set_burst_mode(void __iomem *base, unsigned long flags)
{
	u32 val;
	u32 mode_mask = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	u32 non_burst_sync_pulse = MIPI_DSI_MODE_VIDEO |
		MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	u32 non_burst_sync_event = MIPI_DSI_MODE_VIDEO;

	/*
	 * choose video mode type
	 */
	if ((flags & mode_mask) == non_burst_sync_pulse)
		val = DSI_NON_BURST_SYNC_PULSES;
	else if ((flags & mode_mask) == non_burst_sync_event)
		val = DSI_NON_BURST_SYNC_EVENTS;
	else
		val = DSI_BURST_SYNC_PULSES_1;

	set_reg(base + MIPIDSI_VID_MODE_CFG_OFFSET, val, 2, 0);
}

/*
 * dsi phy reg write function
 */
static void dsi_phy_tst_set(void __iomem *base, u32 reg, u32 val)
{
	u32 reg_write = 0x10000 + reg;

	/*
	 * latch reg first
	 */
	writel(reg_write, base + MIPIDSI_PHY_TST_CTRL1_OFFSET);
	writel(0x02, base + MIPIDSI_PHY_TST_CTRL0_OFFSET);
	writel(0x00, base + MIPIDSI_PHY_TST_CTRL0_OFFSET);

	/*
	 * then latch value
	 */
	writel(val, base + MIPIDSI_PHY_TST_CTRL1_OFFSET);
	writel(0x02, base + MIPIDSI_PHY_TST_CTRL0_OFFSET);
	writel(0x00, base + MIPIDSI_PHY_TST_CTRL0_OFFSET);
}

static void dsi_mipi_init(struct dw_dsi *dsi, char __iomem *mipi_dsi_base)
{
	u32 hline_time = 0;
	u32 hsa_time = 0;
	u32 hbp_time = 0;
	u64 pixel_clk = 0;
	u32 i = 0;
	u32 id = 0;
	unsigned long dw_jiffies = 0;
	u32 tmp = 0;
	bool is_ready = false;
	struct mipi_panel_info *mipi = NULL;
	dss_rect_t rect;
	u32 cmp_stopstate_val = 0;
	u32 lanes;

	WARN_ON(!dsi);
	WARN_ON(!mipi_dsi_base);

	id = dsi->cur_client;
	mipi = &dsi->mipi;

	if (mipi->max_tx_esc_clk == 0) {
		DRM_INFO("max_tx_esc_clk is invalid!");
		mipi->max_tx_esc_clk = DEFAULT_MAX_TX_ESC_CLK;
	}

	memset(&dsi->phy, 0, sizeof(struct mipi_phy_params));
	get_dsi_phy_ctrl(dsi, &dsi->phy);

	rect.x = 0;
	rect.y = 0;
	rect.w = dsi->cur_mode.hdisplay;
	rect.h = dsi->cur_mode.vdisplay;
	lanes = dsi->client[id].lanes - 1;
	/***************Configure the DPHY start**************/

	set_reg(mipi_dsi_base + MIPIDSI_PHY_IF_CFG_OFFSET, lanes, 2, 0);
	set_reg(mipi_dsi_base + MIPIDSI_CLKMGR_CFG_OFFSET, dsi->phy.clk_division, 8, 0);
	set_reg(mipi_dsi_base + MIPIDSI_CLKMGR_CFG_OFFSET, dsi->phy.clk_division, 8, 8);

	outp32(mipi_dsi_base + MIPIDSI_PHY_RSTZ_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000001);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	/* physical configuration PLL I*/
	dsi_phy_tst_set(mipi_dsi_base, 0x14,
		(dsi->phy.rg_pll_fbd_s << 4) + (dsi->phy.rg_pll_enswc << 3) +
		(dsi->phy.rg_pll_enbwt << 2) + dsi->phy.rg_pll_chp);

	/* physical configuration PLL II, M*/
	dsi_phy_tst_set(mipi_dsi_base, 0x15, dsi->phy.rg_pll_fbd_p);

	/* physical configuration PLL III*/
	dsi_phy_tst_set(mipi_dsi_base, 0x16,
		(dsi->phy.rg_pll_cp << 5) + (dsi->phy.rg_pll_lpf_cs << 4) +
		dsi->phy.rg_pll_refsel);

	/* physical configuration PLL IV, N*/
	dsi_phy_tst_set(mipi_dsi_base, 0x17, dsi->phy.rg_pll_pre_p);

	/* sets the analog characteristic of V reference in D-PHY TX*/
	dsi_phy_tst_set(mipi_dsi_base, 0x1D, dsi->phy.rg_vrefsel_vcm);

	/* MISC AFE Configuration*/
	dsi_phy_tst_set(mipi_dsi_base, 0x1E,
		(dsi->phy.rg_pll_cp_p << 5) + (dsi->phy.reload_sel << 4) +
		(dsi->phy.rg_phase_gen_en << 3) + (dsi->phy.rg_band_sel << 2) +
		(dsi->phy.pll_power_down << 1) + dsi->phy.pll_register_override);

	/*reload_command*/
	dsi_phy_tst_set(mipi_dsi_base, 0x1F, dsi->phy.load_command);

	/* pre_delay of clock lane request setting*/
	dsi_phy_tst_set(mipi_dsi_base, 0x20, DSS_REDUCE(dsi->phy.clk_pre_delay));

	/* post_delay of clock lane request setting*/
	dsi_phy_tst_set(mipi_dsi_base, 0x21, DSS_REDUCE(dsi->phy.clk_post_delay));

	/* clock lane timing ctrl - t_lpx*/
	dsi_phy_tst_set(mipi_dsi_base, 0x22, DSS_REDUCE(dsi->phy.clk_t_lpx));

	/* clock lane timing ctrl - t_hs_prepare*/
	dsi_phy_tst_set(mipi_dsi_base, 0x23, DSS_REDUCE(dsi->phy.clk_t_hs_prepare));

	/* clock lane timing ctrl - t_hs_zero*/
	dsi_phy_tst_set(mipi_dsi_base, 0x24, DSS_REDUCE(dsi->phy.clk_t_hs_zero));

	/* clock lane timing ctrl - t_hs_trial*/
	dsi_phy_tst_set(mipi_dsi_base, 0x25, dsi->phy.clk_t_hs_trial);

	for (i = 0; i <= lanes; i++) {
		/* data lane pre_delay*/
		tmp = 0x30 + (i << 4);
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_pre_delay));

		/*data lane post_delay*/
		tmp = 0x31 + (i << 4);
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_post_delay));

		/* data lane timing ctrl - t_lpx*/
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_t_lpx));

		/* data lane timing ctrl - t_hs_prepare*/
		tmp = 0x33 + (i << 4);
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_t_hs_prepare));

		/* data lane timing ctrl - t_hs_zero*/
		tmp = 0x34 + (i << 4);
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_t_hs_zero));

		/* data lane timing ctrl - t_hs_trial*/
		tmp = 0x35 + (i << 4);
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_t_hs_trial));

		/* data lane timing ctrl - t_ta_go*/
		tmp = 0x36 + (i << 4);
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_t_ta_go));

		/* data lane timing ctrl - t_ta_get*/
		tmp = 0x37 + (i << 4);
		dsi_phy_tst_set(mipi_dsi_base, tmp, DSS_REDUCE(dsi->phy.data_t_ta_get));
	}

	outp32(mipi_dsi_base + MIPIDSI_PHY_RSTZ_OFFSET, 0x00000007);

	is_ready = false;
	dw_jiffies = jiffies + HZ / 2;
	do {
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
		if ((tmp & 0x00000001) == 0x00000001) {
			is_ready = true;
			break;
		}
	} while (time_after(dw_jiffies, jiffies));

	if (!is_ready) {
		DRM_INFO("phylock is not ready!MIPIDSI_PHY_STATUS_OFFSET=0x%x.\n",
			tmp);
	}

	if (lanes >= DSI_4_LANES)
		cmp_stopstate_val = (BIT(4) | BIT(7) | BIT(9) | BIT(11));
	else if (lanes >= DSI_3_LANES)
		cmp_stopstate_val = (BIT(4) | BIT(7) | BIT(9));
	else if (lanes >= DSI_2_LANES)
		cmp_stopstate_val = (BIT(4) | BIT(7));
	else
		cmp_stopstate_val = (BIT(4));

	is_ready = false;
	dw_jiffies = jiffies + HZ / 2;
	do {
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
		if ((tmp & cmp_stopstate_val) == cmp_stopstate_val) {
			is_ready = true;
			break;
		}
	} while (time_after(dw_jiffies, jiffies));

	if (!is_ready) {
		DRM_INFO("phystopstateclklane is not ready! MIPIDSI_PHY_STATUS_OFFSET=0x%x.\n",
			tmp);
	}

	/*************************Configure the DPHY end*************************/

	/* phy_stop_wait_time*/
	set_reg(mipi_dsi_base + MIPIDSI_PHY_IF_CFG_OFFSET, dsi->phy.phy_stop_wait_time, 8, 8);

	/*--------------configuring the DPI packet transmission----------------*/
	/*
	** 2. Configure the DPI Interface:
	** This defines how the DPI interface interacts with the controller.
	*/
	set_reg(mipi_dsi_base + MIPIDSI_DPI_VCID_OFFSET, mipi->vc, 2, 0);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_COLOR_CODING_OFFSET, mipi->color_mode, 4, 0);

	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET, dsi->ldi.data_en_plr, 1, 0);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET, dsi->ldi.vsync_plr, 1, 1);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET, dsi->ldi.hsync_plr, 1, 2);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET, 0x0, 1, 3);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET, 0x0, 1, 4);

	/*
	** 3. Select the Video Transmission Mode:
	** This defines how the processor requires the video line to be
	** transported through the DSI link.
	*/
	/* video mode: low power mode*/
	set_reg(mipi_dsi_base + MIPIDSI_VID_MODE_CFG_OFFSET, 0x3f, 6, 8);
	/* set_reg(mipi_dsi_base + MIPIDSI_VID_MODE_CFG_OFFSET, 0x0, 1, 14); */

	/* TODO: fix blank display bug when set backlight*/
	set_reg(mipi_dsi_base + MIPIDSI_DPI_LP_CMD_TIM_OFFSET, 0x4, 8, 16);
	/* video mode: send read cmd by lp mode*/
	set_reg(mipi_dsi_base + MIPIDSI_VID_MODE_CFG_OFFSET, 0x1, 1, 15);

	set_reg(mipi_dsi_base + MIPIDSI_VID_PKT_SIZE_OFFSET, rect.w, 14, 0);

	/* burst mode*/
	dsi_set_burst_mode(mipi_dsi_base, dsi->client[id].mode_flags);
	/* for dsi read, BTA enable*/
	set_reg(mipi_dsi_base + MIPIDSI_PCKHDL_CFG_OFFSET, 0x1, 1, 2);

	/*
	** 4. Define the DPI Horizontal timing configuration:
	**
	** Hsa_time = HSA*(PCLK period/Clk Lane Byte Period);
	** Hbp_time = HBP*(PCLK period/Clk Lane Byte Period);
	** Hline_time = (HSA+HBP+HACT+HFP)*(PCLK period/Clk Lane Byte Period);
	*/
	pixel_clk = dsi->cur_mode.clock * 1000;
	/*htot = dsi->cur_mode.htotal;*/
	/*vtot = dsi->cur_mode.vtotal;*/
	dsi->ldi.h_front_porch = dsi->cur_mode.hsync_start - dsi->cur_mode.hdisplay;
	dsi->ldi.h_back_porch = dsi->cur_mode.htotal - dsi->cur_mode.hsync_end;
	dsi->ldi.h_pulse_width = dsi->cur_mode.hsync_end - dsi->cur_mode.hsync_start;
	dsi->ldi.v_front_porch = dsi->cur_mode.vsync_start - dsi->cur_mode.vdisplay;
	dsi->ldi.v_back_porch = dsi->cur_mode.vtotal - dsi->cur_mode.vsync_end;
	dsi->ldi.v_pulse_width = dsi->cur_mode.vsync_end - dsi->cur_mode.vsync_start;
	if (dsi->ldi.v_pulse_width > 15) {
		DRM_DEBUG_DRIVER("vsw exceeded 15\n");
		dsi->ldi.v_pulse_width = 15;
	}
	hsa_time = dsi->ldi.h_pulse_width * dsi->phy.lane_byte_clk / pixel_clk;
	hbp_time = dsi->ldi.h_back_porch * dsi->phy.lane_byte_clk / pixel_clk;
	hline_time = ROUND1((dsi->ldi.h_pulse_width + dsi->ldi.h_back_porch +
		rect.w + dsi->ldi.h_front_porch) * dsi->phy.lane_byte_clk, pixel_clk);

	DRM_INFO("hsa_time=%d, hbp_time=%d, hline_time=%d\n",
	    hsa_time, hbp_time, hline_time);
	DRM_INFO("lane_byte_clk=%llu, pixel_clk=%llu\n",
	    dsi->phy.lane_byte_clk, pixel_clk);
	set_reg(mipi_dsi_base + MIPIDSI_VID_HSA_TIME_OFFSET, hsa_time, 12, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_HBP_TIME_OFFSET, hbp_time, 12, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_HLINE_TIME_OFFSET, hline_time, 15, 0);

	/* Define the Vertical line configuration*/
	set_reg(mipi_dsi_base + MIPIDSI_VID_VSA_LINES_OFFSET, dsi->ldi.v_pulse_width, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_VBP_LINES_OFFSET, dsi->ldi.v_back_porch, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_VFP_LINES_OFFSET, dsi->ldi.v_front_porch, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_VACTIVE_LINES_OFFSET, rect.h, 14, 0);
	set_reg(mipi_dsi_base + MIPIDSI_TO_CNT_CFG_OFFSET, 0x7FF, 16, 0);

	/* Configure core's phy parameters*/
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_LPCLK_CFG_OFFSET, dsi->phy.clk_lane_lp2hs_time, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_LPCLK_CFG_OFFSET, dsi->phy.clk_lane_hs2lp_time, 10, 16);

	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_RD_CFG_OFFSET, 0x7FFF, 15, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_CFG_OFFSET, dsi->phy.data_lane_lp2hs_time, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_CFG_OFFSET, dsi->phy.data_lane_hs2lp_time, 10, 16);

	/* Waking up Core*/
	set_reg(mipi_dsi_base + MIPIDSI_PWR_UP_OFFSET, 0x1, 1, 0);
}

static void dsi_encoder_disable(struct drm_encoder *encoder)
{
	struct dw_dsi *dsi = encoder_to_dsi(encoder);
	struct dsi_hw_ctx *ctx = dsi->ctx;
	void __iomem *base = ctx->base;

	if (!dsi->enable)
		return;

	dw_dsi_set_mode(dsi, DSI_COMMAND_MODE);
	/* turn off panel's backlight */
	if (dsi->panel && drm_panel_disable(dsi->panel))
		DRM_ERROR("failed to disable panel\n");

	/* turn off panel */
	if (dsi->panel && drm_panel_unprepare(dsi->panel))
		DRM_ERROR("failed to unprepare panel\n");

	writel(0, base + PWR_UP);
	writel(0, base + LPCLK_CTRL);
	writel(0, base + PHY_RSTZ);
	clk_disable_unprepare(ctx->dss_dphy0_ref_clk);
	clk_disable_unprepare(ctx->dss_dphy0_cfg_clk);
	clk_disable_unprepare(ctx->dss_pclk_dsi0_clk);

	dsi->enable = false;
}

static int mipi_dsi_on_sub1(struct dw_dsi *dsi, char __iomem *mipi_dsi_base)
{
	WARN_ON(!mipi_dsi_base);

	/* mipi init */
	dsi_mipi_init(dsi, mipi_dsi_base);
	DRM_INFO("dsi_mipi_init ok\n");
	/* switch to cmd mode */
	set_reg(mipi_dsi_base + MIPIDSI_MODE_CFG_OFFSET, 0x1, 1, 0);
	/* cmd mode: low power mode */
	set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x7f, 7, 8);
	set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0xf, 4, 16);
	set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x1, 1, 24);
	/* disable generate High Speed clock */
	/* delete? */
	set_reg(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET, 0x0, 1, 0);

	return 0;
}

static int mipi_dsi_on_sub2(struct dw_dsi *dsi, char __iomem *mipi_dsi_base)
{
	WARN_ON(!mipi_dsi_base);

	/* switch to video mode */
	set_reg(mipi_dsi_base + MIPIDSI_MODE_CFG_OFFSET, 0x0, 1, 0);

	/* enable EOTP TX */
	set_reg(mipi_dsi_base + MIPIDSI_PCKHDL_CFG_OFFSET, 0x1, 1, 0);

	/* enable generate High Speed clock, continue clock */
	set_reg(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET, 0x1, 2, 0);

	return 0;
}

static void dsi_encoder_enable(struct drm_encoder *encoder)
{
	struct dw_dsi *dsi = encoder_to_dsi(encoder);
	struct dsi_hw_ctx *ctx = dsi->ctx;
	int ret;

	if (dsi->enable)
		return;

	ret = clk_prepare_enable(ctx->dss_dphy0_ref_clk);
	if (ret) {
		DRM_ERROR("fail to enable dss_dphy0_ref_clk: %d\n", ret);
		return;
	}

	ret = clk_prepare_enable(ctx->dss_dphy0_cfg_clk);
	if (ret) {
		DRM_ERROR("fail to enable dss_dphy0_cfg_clk: %d\n", ret);
		return;
	}

	ret = clk_prepare_enable(ctx->dss_pclk_dsi0_clk);
	if (ret) {
		DRM_ERROR("fail to enable dss_pclk_dsi0_clk: %d\n", ret);
		return;
	}

	mipi_dsi_on_sub1(dsi, ctx->base);

	mipi_dsi_on_sub2(dsi, ctx->base);

	/* turn on panel */
	if (dsi->panel && drm_panel_prepare(dsi->panel))
		DRM_ERROR("failed to prepare panel\n");

	/*dw_dsi_set_mode(dsi, DSI_VIDEO_MODE);*/

	/* turn on panel's back light */
	if (dsi->panel && drm_panel_enable(dsi->panel))
		DRM_ERROR("failed to enable panel\n");

	dsi->enable = true;
}

static void dsi_encoder_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	struct dw_dsi *dsi = encoder_to_dsi(encoder);

	drm_mode_copy(&dsi->cur_mode, adj_mode);
}

static int dsi_encoder_atomic_check(struct drm_encoder *encoder,
				    struct drm_crtc_state *crtc_state,
				    struct drm_connector_state *conn_state)
{
	/* do nothing */
	return 0;
}

static const struct drm_encoder_helper_funcs dw_encoder_helper_funcs = {
	.atomic_check	= dsi_encoder_atomic_check,
	.mode_set	= dsi_encoder_mode_set,
	.enable		= dsi_encoder_enable,
	.disable	= dsi_encoder_disable
};

static const struct drm_encoder_funcs dw_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int dw_drm_encoder_init(struct device *dev,
			       struct drm_device *drm_dev,
			       struct drm_encoder *encoder)
{
	int ret;
	u32 crtc_mask = drm_of_find_possible_crtcs(drm_dev, dev->of_node);

	if (!crtc_mask) {
		DRM_ERROR("failed to find crtc mask\n");
		return -EINVAL;
	}

	encoder->possible_crtcs = crtc_mask;
	ret = drm_encoder_init(drm_dev, encoder, &dw_encoder_funcs,
			       DRM_MODE_ENCODER_DSI);
	if (ret) {
		DRM_ERROR("failed to init dsi encoder\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &dw_encoder_helper_funcs);

	return 0;
}

static int dsi_host_attach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *mdsi)
{
	struct dw_dsi *dsi = host_to_dsi(host);
	u32 id = mdsi->channel >= 1 ? OUT_PANEL : OUT_HDMI;

	if (mdsi->lanes < 1 || mdsi->lanes > 4) {
		DRM_ERROR("dsi device params invalid\n");
		return -EINVAL;
	}

	dsi->client[id].lanes = mdsi->lanes;
	dsi->client[id].format = mdsi->format;
	dsi->client[id].mode_flags = mdsi->mode_flags;
	dsi->client[id].phy_clock = mdsi->phy_clock;

	DRM_INFO("host attach, client name=[%s], id=%d\n", mdsi->name, id);

	return 0;
}

static int dsi_host_detach(struct mipi_dsi_host *host,
			   struct mipi_dsi_device *mdsi)
{
	/* do nothing */
	return 0;
}

static int dsi_gen_pkt_hdr_write(void __iomem *base, u32 val)
{
	u32 status;
	int ret;

	ret = readx_poll_timeout(readl, base + CMD_PKT_STATUS, status,
				 !(status & GEN_CMD_FULL), 1000,
				 CMD_PKT_STATUS_TIMEOUT_US);
	if (ret < 0) {
		DRM_ERROR("failed to get available command FIFO\n");
		return ret;
	}

	writel(val, base + GEN_HDR);

	ret = readx_poll_timeout(readl, base + CMD_PKT_STATUS, status,
				 status & (GEN_CMD_EMPTY | GEN_PLD_W_EMPTY),
				 1000, CMD_PKT_STATUS_TIMEOUT_US);
	if (ret < 0) {
		DRM_ERROR("failed to write command FIFO\n");
		return ret;
	}

	return 0;
}

static int dsi_dcs_short_write(void __iomem *base,
			       const struct mipi_dsi_msg *msg)
{
	const u16 *tx_buf = msg->tx_buf;
	u32 val = GEN_HDATA(*tx_buf) | GEN_HTYPE(msg->type);

	if (msg->tx_len > 2) {
		DRM_ERROR("too long tx buf length %zu for short write\n",
			  msg->tx_len);
		return -EINVAL;
	}

	return dsi_gen_pkt_hdr_write(base, val);
}

static int dsi_dcs_long_write(void __iomem *base,
			      const struct mipi_dsi_msg *msg)
{
	const u32 *tx_buf = msg->tx_buf;
	int len = msg->tx_len, pld_data_bytes = sizeof(*tx_buf), ret;
	u32 val = GEN_HDATA(msg->tx_len) | GEN_HTYPE(msg->type);
	u32 remainder = 0;
	u32 status;

	if (msg->tx_len < 3) {
		DRM_ERROR("wrong tx buf length %zu for long write\n",
			  msg->tx_len);
		return -EINVAL;
	}

	while (DIV_ROUND_UP(len, pld_data_bytes)) {
		if (len < pld_data_bytes) {
			memcpy(&remainder, tx_buf, len);
			writel(remainder, base + GEN_PLD_DATA);
			len = 0;
		} else {
			writel(*tx_buf, base + GEN_PLD_DATA);
			tx_buf++;
			len -= pld_data_bytes;
		}

		ret = readx_poll_timeout(readl, base + CMD_PKT_STATUS,
					 status, !(status & GEN_PLD_W_FULL), 1000,
					 CMD_PKT_STATUS_TIMEOUT_US);
		if (ret < 0) {
			DRM_ERROR("failed to get available write payload FIFO\n");
			return ret;
		}
	}

	return dsi_gen_pkt_hdr_write(base, val);
}

static ssize_t dsi_host_transfer(struct mipi_dsi_host *host,
				    const struct mipi_dsi_msg *msg)
{
	struct dw_dsi *dsi = host_to_dsi(host);
	struct dsi_hw_ctx *ctx = dsi->ctx;
	void __iomem *base = ctx->base;
	int ret;

	switch (msg->type) {
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		ret = dsi_dcs_short_write(base, msg);
		break;
	case MIPI_DSI_DCS_LONG_WRITE:
		ret = dsi_dcs_long_write(base, msg);
		break;
	default:
		DRM_ERROR("unsupported message type\n");
		ret = -EINVAL;
	}

	return ret;
}

static const struct mipi_dsi_host_ops dsi_host_ops = {
	.attach = dsi_host_attach,
	.detach = dsi_host_detach,
	.transfer = dsi_host_transfer,
};

static int dsi_host_init(struct device *dev, struct dw_dsi *dsi)
{
	struct mipi_dsi_host *host = &dsi->host;
	struct mipi_panel_info *mipi = &dsi->mipi;
	int ret;

	host->dev = dev;
	host->ops = &dsi_host_ops;

	mipi->max_tx_esc_clk = 10 * 1000000UL;
	mipi->vc = 0;
	mipi->color_mode = DSI_24BITS_1;
	mipi->clk_post_adjust = 120;
	mipi->clk_pre_adjust= 0;
	mipi->clk_t_hs_prepare_adjust= 0;
	mipi->clk_t_lpx_adjust= 0;
	mipi->clk_t_hs_trial_adjust= 0;
	mipi->clk_t_hs_exit_adjust= 0;
	mipi->clk_t_hs_zero_adjust= 0;

	dsi->ldi.data_en_plr = 0;
	dsi->ldi.vsync_plr = 0;
	dsi->ldi.hsync_plr = 0;

	ret = mipi_dsi_host_register(host);
	if (ret) {
		DRM_ERROR("failed to register dsi host\n");
		return ret;
	}

	return 0;
}

static int dsi_bridge_init(struct drm_device *dev, struct dw_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct drm_bridge *bridge = dsi->bridge;
	int ret;

	/* associate the bridge to dsi encoder */
	bridge->encoder = encoder;

	ret = drm_bridge_attach(dev, bridge);
	if (ret) {
		DRM_ERROR("failed to attach external bridge\n");
		return ret;
	}

	return 0;
}

static int dsi_connector_get_modes(struct drm_connector *connector)
{
	struct dw_dsi *dsi = connector_to_dsi(connector);

	return drm_panel_get_modes(dsi->panel);
}

static enum drm_mode_status
dsi_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	enum drm_mode_status mode_status = MODE_OK;

	return mode_status;
}

static struct drm_encoder *
dsi_connector_best_encoder(struct drm_connector *connector)
{
	struct dw_dsi *dsi = connector_to_dsi(connector);

	return &dsi->encoder;
}

static struct drm_connector_helper_funcs dsi_connector_helper_funcs = {
	.get_modes = dsi_connector_get_modes,
	.mode_valid = dsi_connector_mode_valid,
	.best_encoder = dsi_connector_best_encoder,
};

static enum drm_connector_status
dsi_connector_detect(struct drm_connector *connector, bool force)
{
	struct dw_dsi *dsi = connector_to_dsi(connector);
	enum drm_connector_status status;

	status = dsi->cur_client == OUT_PANEL ?	connector_status_connected :
		connector_status_disconnected;

	return status;
}

static void dsi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static struct drm_connector_funcs dsi_atomic_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = dsi_connector_detect,
	.destroy = dsi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int dsi_connector_init(struct drm_device *dev, struct dw_dsi *dsi)
{
	struct drm_encoder *encoder = &dsi->encoder;
	struct drm_connector *connector = &dsi->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	drm_connector_helper_add(connector,
				 &dsi_connector_helper_funcs);

	ret = drm_connector_init(dev, &dsi->connector,
				 &dsi_atomic_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret)
		return ret;

	ret = drm_mode_connector_attach_encoder(connector, encoder);
	if (ret)
		return ret;

	ret = drm_panel_attach(dsi->panel, connector);
	if (ret)
		return ret;

	DRM_INFO("connector init\n");
	return 0;
}
static int dsi_bind(struct device *dev, struct device *master, void *data)
{
	struct dsi_data *ddata = dev_get_drvdata(dev);
	struct dw_dsi *dsi = &ddata->dsi;
	struct drm_device *drm_dev = data;
	int ret;

	ret = dw_drm_encoder_init(dev, drm_dev, &dsi->encoder);
	if (ret)
		return ret;

	if (dsi->bridge) {
		ret = dsi_bridge_init(drm_dev, dsi);
		if (ret)
			return ret;
	}

	if (dsi->panel) {
		ret = dsi_connector_init(drm_dev, dsi);
		if (ret)
			return ret;
	}

	return 0;
}

static void dsi_unbind(struct device *dev, struct device *master, void *data)
{
	/* do nothing */
}

static const struct component_ops dsi_ops = {
	.bind	= dsi_bind,
	.unbind	= dsi_unbind,
};

static int dsi_parse_bridge_endpoint(struct dw_dsi *dsi,
				     struct device_node *endpoint)
{
	struct device_node *bridge_node;
	struct drm_bridge *bridge;

	bridge_node = of_graph_get_remote_port_parent(endpoint);
	if (!bridge_node) {
		DRM_ERROR("no valid bridge node\n");
		return -ENODEV;
	}
	of_node_put(bridge_node);

	bridge = of_drm_find_bridge(bridge_node);
	if (!bridge) {
		DRM_INFO("wait for external HDMI bridge driver.\n");
		return -EPROBE_DEFER;
	}
	dsi->bridge = bridge;

	return 0;
}

static int dsi_parse_panel_endpoint(struct dw_dsi *dsi,
				    struct device_node *endpoint)
{
	struct device_node *panel_node;
	struct drm_panel *panel;

	panel_node = of_graph_get_remote_port_parent(endpoint);
	if (!panel_node) {
		DRM_ERROR("no valid panel node\n");
		return -ENODEV;
	}
	of_node_put(panel_node);

	panel = of_drm_find_panel(panel_node);
	if (!panel) {
		DRM_DEBUG_DRIVER("skip this panel endpoint.\n");
		return 0;
	}
	dsi->panel = panel;

	return 0;
}

static int dsi_parse_endpoint(struct dw_dsi *dsi,
			      struct device_node *np,
			      enum dsi_output_client client)
{
	struct device_node *ep_node;
	struct of_endpoint ep;
	int ret = 0;

	if (client == OUT_MAX)
		return -EINVAL;

	for_each_endpoint_of_node(np, ep_node) {
		ret = of_graph_parse_endpoint(ep_node, &ep);
		if (ret) {
			of_node_put(ep_node);
			return ret;
		}

		/* skip dsi input port, port == 0 is input port */
		if (ep.port == 0)
			continue;

		/* parse bridge endpoint */
		if (client == OUT_HDMI) {
			if (ep.id == 0) {
				ret = dsi_parse_bridge_endpoint(dsi, ep_node);
				if (dsi->bridge)
					break;
			}
		} else { /* parse panel endpoint */
			if (ep.id > 0) {
				ret = dsi_parse_panel_endpoint(dsi, ep_node);
				if (dsi->panel)
					break;
			}
		}

		if (ret) {
			of_node_put(ep_node);
			return ret;
		}
	}

	if (!dsi->bridge && !dsi->panel) {
		DRM_ERROR("at least one bridge or panel node is required\n");
		return -ENODEV;
	}

	return 0;
}

static int dsi_parse_dt(struct platform_device *pdev, struct dw_dsi *dsi)
{
	struct dsi_hw_ctx *ctx = dsi->ctx;
	int ret = 0;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_DSI_NAME);
	if (!np) {
			DRM_ERROR("NOT FOUND device node %s!\n",
				    DTS_COMP_DSI_NAME);
			return -ENXIO;
	}

	ctx->base = of_iomap(np, 0);
	if (!(ctx->base)) {
			DRM_ERROR ("failed to get base resource.\n");
			return -ENXIO;
	}

	ctx->peri_crg_base = of_iomap(np, 1);
	if (!(ctx->peri_crg_base)) {
			DRM_ERROR ("failed to get peri_crg_base resource.\n");
			return -ENXIO;
	}

	dsi->gpio_mux = devm_gpiod_get(&pdev->dev, "mux", GPIOD_OUT_HIGH);
	if (IS_ERR(dsi->gpio_mux))
		return PTR_ERR(dsi->gpio_mux);
	/* set dsi default output to panel */
	dsi->cur_client = OUT_PANEL;

	/*dis-reset*/
	/*ip_reset_dis_dsi0, ip_reset_dis_dsi1*/
	outp32(ctx->peri_crg_base + PERRSTDIS3, 0x30000000);

	ctx->dss_dphy0_ref_clk = devm_clk_get(&pdev->dev, "clk_txdphy0_ref");
	if (IS_ERR(ctx->dss_dphy0_ref_clk)) {
		DRM_ERROR("failed to get dss_dphy0_ref_clk clock\n");
		return PTR_ERR(ctx->dss_dphy0_ref_clk);
	}

	ret = clk_set_rate(ctx->dss_dphy0_ref_clk, DEFAULT_MIPI_CLK_RATE);
	if (ret < 0) {
		DRM_ERROR("dss_dphy0_ref_clk clk_set_rate(%lu) failed, error=%d!\n",
			DEFAULT_MIPI_CLK_RATE, ret);
		return -EINVAL;
	}

	DRM_DEBUG("dss_dphy0_ref_clk:[%lu]->[%lu].\n",
		DEFAULT_MIPI_CLK_RATE, clk_get_rate(ctx->dss_dphy0_ref_clk));

	ctx->dss_dphy0_cfg_clk = devm_clk_get(&pdev->dev, "clk_txdphy0_cfg");
	if (IS_ERR(ctx->dss_dphy0_cfg_clk)) {
		DRM_ERROR("failed to get dss_dphy0_cfg_clk clock\n");
		return PTR_ERR(ctx->dss_dphy0_cfg_clk);
	}

	ret = clk_set_rate(ctx->dss_dphy0_cfg_clk, DEFAULT_MIPI_CLK_RATE);
	if (ret < 0) {
		DRM_ERROR("dss_dphy0_cfg_clk clk_set_rate(%lu) failed, error=%d!\n",
			DEFAULT_MIPI_CLK_RATE, ret);
		return -EINVAL;
	}

	DRM_DEBUG("dss_dphy0_cfg_clk:[%lu]->[%lu].\n",
		DEFAULT_MIPI_CLK_RATE, clk_get_rate(ctx->dss_dphy0_cfg_clk));

	ctx->dss_pclk_dsi0_clk = devm_clk_get(&pdev->dev, "pclk_dsi0");
	if (IS_ERR(ctx->dss_pclk_dsi0_clk)) {
		DRM_ERROR("failed to get dss_pclk_dsi0_clk clock\n");
		return PTR_ERR(ctx->dss_pclk_dsi0_clk);
	}

	return 0;
}

static int dsi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct dsi_data *data;
	struct dw_dsi *dsi;
	struct dsi_hw_ctx *ctx;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		DRM_ERROR("failed to allocate dsi data.\n");
		return -ENOMEM;
	}
	dsi = &data->dsi;
	ctx = &data->ctx;
	dsi->ctx = ctx;

	/* parse HDMI bridge endpoint */
	ret = dsi_parse_endpoint(dsi, np, OUT_HDMI);
	if (ret)
		return ret;

	ret = dsi_host_init(dev, dsi);
	if (ret)
		return ret;

	/* parse panel endpoint */
	ret = dsi_parse_endpoint(dsi, np, OUT_PANEL);
	if (ret)
		goto err_host_unregister;

	ret = dsi_parse_dt(pdev, dsi);
	if (ret)
		goto err_host_unregister;

	platform_set_drvdata(pdev, data);

	ret = component_add(dev, &dsi_ops);
	if (ret)
		goto err_host_unregister;

	return 0;

err_host_unregister:
	mipi_dsi_host_unregister(&dsi->host);
	return ret;
}

static int dsi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dsi_ops);

	return 0;
}

static const struct of_device_id dsi_of_match[] = {
	{.compatible = "hisilicon,hi3660-dsi"},
	{ }
};
MODULE_DEVICE_TABLE(of, dsi_of_match);

static struct platform_driver dsi_driver = {
	.probe = dsi_probe,
	.remove = dsi_remove,
	.driver = {
		.name = "dw-dsi",
		.of_match_table = dsi_of_match,
	},
};

module_platform_driver(dsi_driver);

MODULE_DESCRIPTION("DesignWare MIPI DSI Host Controller v1.02 driver");
MODULE_LICENSE("GPL v2");

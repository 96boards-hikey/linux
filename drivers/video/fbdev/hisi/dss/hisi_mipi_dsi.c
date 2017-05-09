/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */

#include "hisi_mipi_dsi.h"

#define DEFAULT_MAX_TX_ESC_CLK	(10 * 1000000UL)
#define DEFAULT_MIPI_CLK_RATE	(192 * 100000L)
#define DEFAULT_PCLK_DSI_RATE	(120 * 1000000L)

#define ROUND1(x,y)	((x) / (y) + ((x) % (y) > 0 ? 1 : 0))
#define DSS_REDUCE(x)	((x) > 0 ? ((x) - 1) : (x))

struct dsi_phy_seq_info {
	uint32_t min_range;
	uint32_t max_range;
	uint32_t rg_pll_vco_750M;
	uint32_t rg_hstx_ckg_sel;
};

struct dsi_phy_seq_info dphy_seq_info[] = {
	{47, 94, 0, 7},
	{94, 188, 0, 6},
	{188, 375, 0, 5},
	{375, 750, 0, 4},
	{750, 1500, 0, 0}
};

static void get_dsi_phy_ctrl(struct hisi_fb_data_type *hisifd,
			     struct mipi_dsi_phy_ctrl *phy_ctrl)
{
	struct hisi_panel_info *pinfo = NULL;
	uint32_t dsi_bit_clk = 0;

	uint32_t ui = 0;
	uint32_t m_pll = 0;
	uint32_t n_pll = 0;
	uint32_t m_n_fract = 0;
	uint32_t m_n_int = 0;
	uint64_t lane_clock = 0;
	uint64_t vco_div = 1;

	uint32_t accuracy = 0;
	uint32_t unit_tx_byte_clk_hs = 0;
	uint32_t clk_post = 0;
	uint32_t clk_pre = 0;
	uint32_t clk_t_hs_exit = 0;
	uint32_t clk_pre_delay = 0;
	uint32_t clk_t_hs_prepare = 0;
	uint32_t clk_t_lpx = 0;
	uint32_t clk_t_hs_zero = 0;
	uint32_t clk_t_hs_trial = 0;
	uint32_t data_post_delay = 0;
	uint32_t data_t_hs_prepare = 0;
	uint32_t data_t_hs_zero = 0;
	uint32_t data_t_hs_trial = 0;
	uint32_t data_t_lpx = 0;
	uint32_t clk_pre_delay_reality = 0;
	uint32_t clk_t_hs_zero_reality = 0;
	uint32_t clk_post_delay_reality = 0;
	uint32_t data_t_hs_zero_reality = 0;
	uint32_t data_post_delay_reality = 0;
	uint32_t data_pre_delay_reality = 0;

	BUG_ON(phy_ctrl == NULL);
	BUG_ON(hisifd == NULL);
	pinfo = &(hisifd->panel_info);

	dsi_bit_clk = pinfo->mipi.dsi_bit_clk_upt;
	lane_clock = 2 * dsi_bit_clk;
	HISI_FB_DEBUG("Expected : lane_clock = %llu M\n", lane_clock);

	/************************  PLL parameters config  *********************/
	if ((320 <= lane_clock) && (lane_clock <= 2500)) {
		phy_ctrl->rg_band_sel = 0;
		vco_div = 1;
	} else if ((80 <= lane_clock) && (lane_clock < 320)) {
		phy_ctrl->rg_band_sel = 1;
		vco_div = 4;
	} else {
		HISI_FB_ERR
		    ("80M <= lane_clock< = 2500M, not support lane_clock = %llu M\n",
		     lane_clock);
	}

	m_n_int = lane_clock * vco_div * 1000000UL / DEFAULT_MIPI_CLK_RATE;
	m_n_fract = ((lane_clock * vco_div * 1000000UL * 1000UL /
	      DEFAULT_MIPI_CLK_RATE) % 1000) * 10 / 1000;

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

	if (m_pll <= 8) {
		phy_ctrl->rg_pll_fbd_s = 1;
		phy_ctrl->rg_pll_enswc = 0;

		if (m_pll % 2 == 0) {
			phy_ctrl->rg_pll_fbd_p = m_pll / 2;
		} else {
			if (n_pll == 1) {
				n_pll *= 2;
				phy_ctrl->rg_pll_fbd_p = (m_pll * 2) / 2;
			} else {
				HISI_FB_ERR
				    ("phy m_pll not support!m_pll = %d\n", m_pll);
				return;
			}
		}
	} else if (m_pll <= 300) {
		if (m_pll % 2 == 0) {
			phy_ctrl->rg_pll_enswc = 0;
		} else {
			phy_ctrl->rg_pll_enswc = 1;
		}
		phy_ctrl->rg_pll_fbd_s = 1;
		phy_ctrl->rg_pll_fbd_p = m_pll / 2;
	} else if (m_pll <= 315) {
		phy_ctrl->rg_pll_fbd_p = 150;
		phy_ctrl->rg_pll_fbd_s = m_pll - 2 * phy_ctrl->rg_pll_fbd_p;
		phy_ctrl->rg_pll_enswc = 1;
	} else {
		HISI_FB_ERR("phy m_pll not support!m_pll = %d\n", m_pll);
		return;
	}

	phy_ctrl->rg_pll_pre_p = n_pll;

	lane_clock = m_pll * (DEFAULT_MIPI_CLK_RATE / n_pll) / vco_div;
	HISI_FB_DEBUG("Config : lane_clock = %llu\n", lane_clock);

	phy_ctrl->rg_pll_cp = 1;
	phy_ctrl->rg_pll_cp_p = 3;

	phy_ctrl->rg_pll_enbwt = 0;
	phy_ctrl->rg_pll_chp = 0;

	phy_ctrl->rg_pll_lpf_cs = 0;
	phy_ctrl->rg_pll_refsel = 1;

	phy_ctrl->reload_sel = 1;
	phy_ctrl->rg_phase_gen_en = 1;
	phy_ctrl->pll_power_down = 0;
	phy_ctrl->pll_register_override = 1;

	phy_ctrl->rg_vrefsel_vcm = 0x55;
	if (pinfo->mipi.rg_vrefsel_vcm_clk_adjust != 0)
		phy_ctrl->rg_vrefsel_vcm = (phy_ctrl->rg_vrefsel_vcm & 0x0F) |
		    ((pinfo->mipi.rg_vrefsel_vcm_clk_adjust & 0x0F) << 4);

	if (pinfo->mipi.rg_vrefsel_vcm_data_adjust != 0)
		phy_ctrl->rg_vrefsel_vcm = (phy_ctrl->rg_vrefsel_vcm & 0xF0) |
		    (pinfo->mipi.rg_vrefsel_vcm_data_adjust & 0x0F);

	phy_ctrl->load_command = 0x5A;

	/********************  clock/data lane parameters config  ******************/
	accuracy = 10;
	ui = 10 * 1000000000UL * accuracy / lane_clock;
	unit_tx_byte_clk_hs = 8 * ui;

	clk_post = 600 * accuracy + 52 * ui + pinfo->mipi.clk_post_adjust * ui;

	clk_pre = 8 * ui + pinfo->mipi.clk_pre_adjust * ui;

	clk_t_hs_exit = 1000 * accuracy + pinfo->mipi.clk_t_hs_exit_adjust * ui;

	clk_pre_delay = 0 + pinfo->mipi.clk_pre_delay_adjust * ui;

	clk_t_hs_trial =
	    600 * accuracy + 3 * unit_tx_byte_clk_hs +
	    pinfo->mipi.clk_t_hs_trial_adjust * ui;

	if (pinfo->mipi.clk_t_hs_prepare_adjust == 0)
		pinfo->mipi.clk_t_hs_prepare_adjust = 43;

	clk_t_hs_prepare =
	    ((380 * accuracy + pinfo->mipi.clk_t_hs_prepare_adjust * ui) <=
	     (950 * accuracy - 8 * ui)) ? (380 * accuracy +
					   pinfo->mipi.clk_t_hs_prepare_adjust *
					   ui) : (950 * accuracy - 8 * ui);

	data_post_delay = 0 + pinfo->mipi.data_post_delay_adjust * ui;

	data_t_hs_trial =
	    ((600 * accuracy + 4 * ui) >=
	     (8 * ui) ? (600 * accuracy + 4 * ui) : (8 * ui)) + 8 * ui +
	    3 * unit_tx_byte_clk_hs + pinfo->mipi.data_t_hs_trial_adjust * ui;

	if (pinfo->mipi.data_t_hs_prepare_adjust == 0)
		pinfo->mipi.data_t_hs_prepare_adjust = 35;

	data_t_hs_prepare =
	    ((400 * accuracy + 4 * ui +
	      pinfo->mipi.data_t_hs_prepare_adjust * ui) <=
	     (850 * accuracy + 6 * ui - 8 * ui)) ? (400 * accuracy + 4 * ui +
						    pinfo->mipi.data_t_hs_prepare_adjust *
						    ui) : (850 * accuracy + 6 * ui - 8 * ui);

	clk_t_lpx = (((2000 * accuracy - clk_t_hs_prepare) >= 500 * accuracy) ?
		     ((2000 * accuracy - clk_t_hs_prepare)) : (500 * accuracy)) +
				pinfo->mipi.clk_t_lpx_adjust * ui;

	clk_t_hs_zero =
	    3000 * accuracy - clk_t_hs_prepare + 3 * unit_tx_byte_clk_hs +
	    pinfo->mipi.clk_t_hs_zero_adjust * ui;

	data_t_lpx = clk_t_lpx + pinfo->mipi.data_t_lpx_adjust * ui;

	data_t_hs_zero = 1450 * accuracy + 10 * ui - data_t_hs_prepare +
	    3 * unit_tx_byte_clk_hs + pinfo->mipi.data_t_hs_zero_adjust * ui;

	phy_ctrl->clk_pre_delay = ROUND1(clk_pre_delay, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_hs_prepare =
	    ROUND1(clk_t_hs_prepare, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_lpx = ROUND1(clk_t_lpx, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_hs_zero = ROUND1(clk_t_hs_zero, unit_tx_byte_clk_hs);
	phy_ctrl->clk_t_hs_trial = ROUND1(clk_t_hs_trial, unit_tx_byte_clk_hs);

	phy_ctrl->data_post_delay =
	    ROUND1(data_post_delay, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_hs_prepare =
	    ROUND1(data_t_hs_prepare, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_lpx = ROUND1(data_t_lpx, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_hs_zero = ROUND1(data_t_hs_zero, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_hs_trial =
	    ROUND1(data_t_hs_trial, unit_tx_byte_clk_hs);
	phy_ctrl->data_t_ta_go = 4;
	phy_ctrl->data_t_ta_get = 5;

	clk_pre_delay_reality = phy_ctrl->clk_pre_delay + 2;
	clk_t_hs_zero_reality = phy_ctrl->clk_t_hs_zero + 8;
	data_t_hs_zero_reality = phy_ctrl->data_t_hs_zero + 4;
	data_post_delay_reality = phy_ctrl->data_post_delay + 4;

	phy_ctrl->clk_post_delay =
	    phy_ctrl->data_t_hs_trial + ROUND1(clk_post, unit_tx_byte_clk_hs);
	phy_ctrl->data_pre_delay =
	    clk_pre_delay_reality + phy_ctrl->clk_t_lpx +
	    phy_ctrl->clk_t_hs_prepare + clk_t_hs_zero_reality +
	    ROUND1(clk_pre, unit_tx_byte_clk_hs);

	clk_post_delay_reality = phy_ctrl->clk_post_delay + 4;
	data_pre_delay_reality = phy_ctrl->data_pre_delay + 2;

	phy_ctrl->clk_lane_lp2hs_time =
	    clk_pre_delay_reality + phy_ctrl->clk_t_lpx +
	    phy_ctrl->clk_t_hs_prepare + clk_t_hs_zero_reality + 3;
	phy_ctrl->clk_lane_hs2lp_time =
	    clk_post_delay_reality + phy_ctrl->clk_t_hs_trial + 3;
	phy_ctrl->data_lane_lp2hs_time =
	    data_pre_delay_reality + phy_ctrl->data_t_lpx +
	    phy_ctrl->data_t_hs_prepare + data_t_hs_zero_reality + 3;
	phy_ctrl->data_lane_hs2lp_time =
	    data_post_delay_reality + phy_ctrl->data_t_hs_trial + 3;
	phy_ctrl->phy_stop_wait_time =
	    clk_post_delay_reality + phy_ctrl->clk_t_hs_trial +
	    ROUND1(clk_t_hs_exit, unit_tx_byte_clk_hs) -
	    (data_post_delay_reality + phy_ctrl->data_t_hs_trial) + 3;

	phy_ctrl->lane_byte_clk = lane_clock / 8;
	phy_ctrl->clk_division =
	    (((phy_ctrl->lane_byte_clk / 2) % pinfo->mipi.max_tx_esc_clk) >
	     0) ? (phy_ctrl->lane_byte_clk / 2 / pinfo->mipi.max_tx_esc_clk +
		   1) : (phy_ctrl->lane_byte_clk / 2 /
			 pinfo->mipi.max_tx_esc_clk);

	HISI_FB_DEBUG("PHY clock_lane and data_lane config : \n"
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
		      phy_ctrl->data_t_ta_go, phy_ctrl->data_t_ta_get);
	HISI_FB_DEBUG("clk_lane_lp2hs_time=%u\n"
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

static uint32_t mipi_pixel_clk(struct hisi_fb_data_type *hisifd)
{
	struct hisi_panel_info *pinfo = NULL;

	BUG_ON(hisifd == NULL);
	pinfo = &(hisifd->panel_info);

	if (pinfo->pxl_clk_rate_div == 0) {
		return pinfo->pxl_clk_rate;
	}

	if ((pinfo->ifbc_type == IFBC_TYPE_NONE) && !is_dual_mipi_panel(hisifd)) {
		pinfo->pxl_clk_rate_div = 1;
	}

	return pinfo->pxl_clk_rate / pinfo->pxl_clk_rate_div;
}

static void mipi_init(struct hisi_fb_data_type *hisifd, char __iomem *mipi_dsi_base)
{
	uint32_t hline_time = 0;
	uint32_t hsa_time = 0;
	uint32_t hbp_time = 0;
	uint64_t pixel_clk = 0;
	uint32_t i = 0;
	unsigned long dw_jiffies = 0;
	uint32_t tmp = 0;
	bool is_ready = false;
	struct hisi_panel_info *pinfo = NULL;
	dss_rect_t rect;
	uint32_t cmp_stopstate_val = 0;

	BUG_ON(hisifd == NULL);
	BUG_ON(mipi_dsi_base == NULL);

	pinfo = &(hisifd->panel_info);

	if (pinfo->mipi.max_tx_esc_clk == 0) {
		HISI_FB_ERR("fb%d, max_tx_esc_clk is invalid!", hisifd->index);
		pinfo->mipi.max_tx_esc_clk = DEFAULT_MAX_TX_ESC_CLK;
	}

	memset(&(pinfo->dsi_phy_ctrl), 0, sizeof(struct mipi_dsi_phy_ctrl));
	get_dsi_phy_ctrl(hisifd, &(pinfo->dsi_phy_ctrl));

	rect.x = 0;
	rect.y = 0;
	rect.w = pinfo->xres;
	rect.h = pinfo->yres;

	mipi_ifbc_get_rect(hisifd, &rect);

	/*************************Configure the DPHY start*************************/
	set_reg(mipi_dsi_base + MIPIDSI_PHY_IF_CFG_OFFSET,
		pinfo->mipi.lane_nums, 2, 0);
	set_reg(mipi_dsi_base + MIPIDSI_CLKMGR_CFG_OFFSET,
		pinfo->dsi_phy_ctrl.clk_division, 8, 0);
	set_reg(mipi_dsi_base + MIPIDSI_CLKMGR_CFG_OFFSET,
		pinfo->dsi_phy_ctrl.clk_division, 8, 8);

	outp32(mipi_dsi_base + MIPIDSI_PHY_RSTZ_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000001);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010014);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       (pinfo->dsi_phy_ctrl.rg_pll_fbd_s << 4) +
	       (pinfo->dsi_phy_ctrl.rg_pll_enswc << 3) +
	       (pinfo->dsi_phy_ctrl.rg_pll_enbwt << 2) +
	       pinfo->dsi_phy_ctrl.rg_pll_chp);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010015);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       pinfo->dsi_phy_ctrl.rg_pll_fbd_p);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010016);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       (pinfo->dsi_phy_ctrl.rg_pll_cp << 5) +
	       (pinfo->dsi_phy_ctrl.rg_pll_lpf_cs << 4) +
	       pinfo->dsi_phy_ctrl.rg_pll_refsel);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010017);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       pinfo->dsi_phy_ctrl.rg_pll_pre_p);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x0001001D);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       pinfo->dsi_phy_ctrl.rg_vrefsel_vcm);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x0001001E);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       (pinfo->dsi_phy_ctrl.rg_pll_cp_p << 5) +
	       (pinfo->dsi_phy_ctrl.reload_sel << 4) +
	       (pinfo->dsi_phy_ctrl.rg_phase_gen_en << 3) +
	       (pinfo->dsi_phy_ctrl.rg_band_sel << 2) +
	       (pinfo->dsi_phy_ctrl.pll_power_down << 1) +
	       pinfo->dsi_phy_ctrl.pll_register_override);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x0001001F);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       pinfo->dsi_phy_ctrl.load_command);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010020);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       DSS_REDUCE(pinfo->dsi_phy_ctrl.clk_pre_delay));
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010021);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       DSS_REDUCE(pinfo->dsi_phy_ctrl.clk_post_delay));
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010022);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       DSS_REDUCE(pinfo->dsi_phy_ctrl.clk_t_lpx));
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010023);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       DSS_REDUCE(pinfo->dsi_phy_ctrl.clk_t_hs_prepare));
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010024);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       DSS_REDUCE(pinfo->dsi_phy_ctrl.clk_t_hs_zero));
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, 0x00010025);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
	       pinfo->dsi_phy_ctrl.clk_t_hs_trial);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000002);
	outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET, 0x00000000);

	for (i = 0; i <= pinfo->mipi.lane_nums; i++) {
		tmp = 0x10030 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       DSS_REDUCE(pinfo->dsi_phy_ctrl.data_pre_delay));
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);

		tmp = 0x10031 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       DSS_REDUCE(pinfo->dsi_phy_ctrl.data_post_delay));
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);

		tmp = 0x10032 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       DSS_REDUCE(pinfo->dsi_phy_ctrl.data_t_lpx));
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);

		tmp = 0x10033 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       DSS_REDUCE(pinfo->dsi_phy_ctrl.data_t_hs_prepare));
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);

		tmp = 0x10034 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       DSS_REDUCE(pinfo->dsi_phy_ctrl.data_t_hs_zero));
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);

		tmp = 0x10035 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       pinfo->dsi_phy_ctrl.data_t_hs_trial);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);

		tmp = 0x10036 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       DSS_REDUCE(pinfo->dsi_phy_ctrl.data_t_ta_go));
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);

		tmp = 0x10037 + (i << 4);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET, tmp);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL1_OFFSET,
		       DSS_REDUCE(pinfo->dsi_phy_ctrl.data_t_ta_get));
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000002);
		outp32(mipi_dsi_base + MIPIDSI_PHY_TST_CTRL0_OFFSET,
		       0x00000000);
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
		HISI_FB_INFO
		    ("fb%d, phylock is not ready!MIPIDSI_PHY_STATUS_OFFSET=0x%x.\n",
		     hisifd->index, tmp);
	}

	if (pinfo->mipi.lane_nums >= DSI_4_LANES) {
		cmp_stopstate_val = (BIT(4) | BIT(7) | BIT(9) | BIT(11));
	} else if (pinfo->mipi.lane_nums >= DSI_3_LANES) {
		cmp_stopstate_val = (BIT(4) | BIT(7) | BIT(9));
	} else if (pinfo->mipi.lane_nums >= DSI_2_LANES) {
		cmp_stopstate_val = (BIT(4) | BIT(7));
	} else {
		cmp_stopstate_val = (BIT(4));
	}

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
		HISI_FB_INFO
		    ("fb%d, phystopstateclklane is not ready! "
		     "MIPIDSI_PHY_STATUS_OFFSET=0x%x.\n",
		     hisifd->index, tmp);
	}

	/*************************Configure the DPHY end*************************/

	if (is_mipi_cmd_panel(hisifd)) {

		set_reg(mipi_dsi_base + MIPIDSI_MODE_CFG_OFFSET, 0x1, 1, 0);
		set_reg(mipi_dsi_base + MIPIDSI_EDPI_CMD_SIZE_OFFSET, rect.w, 16, 0);

		if (pinfo->mipi.hs_wr_to_time == 0) {
			set_reg(mipi_dsi_base + MIPIDSI_HS_WR_TO_CNT_OFFSET,
				0x1000002, 25, 0);
		} else {
			set_reg(mipi_dsi_base + MIPIDSI_HS_WR_TO_CNT_OFFSET,
				(0x1 << 24) | (pinfo->mipi.hs_wr_to_time *
				 pinfo->dsi_phy_ctrl.lane_byte_clk / 1000000000UL), 25, 0);
		}
	}

	set_reg(mipi_dsi_base + MIPIDSI_PHY_IF_CFG_OFFSET,
		pinfo->dsi_phy_ctrl.phy_stop_wait_time, 8, 8);

	/*
	 ** 2. Configure the DPI Interface:
	 ** This defines how the DPI interface interacts with the controller.
	 */
	set_reg(mipi_dsi_base + MIPIDSI_DPI_VCID_OFFSET, pinfo->mipi.vc, 2, 0);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_COLOR_CODING_OFFSET,
		pinfo->mipi.color_mode, 4, 0);

	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET,
		pinfo->ldi.data_en_plr, 1, 0);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET,
		pinfo->ldi.vsync_plr, 1, 1);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET,
		pinfo->ldi.hsync_plr, 1, 2);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET, 0x0, 1, 3);
	set_reg(mipi_dsi_base + MIPIDSI_DPI_CFG_POL_OFFSET, 0x0, 1, 4);

	/*
	 ** 3. Select the Video Transmission Mode:
	 ** This defines how the processor requires the video line to be
	 ** transported through the DSI link.
	 */

	set_reg(mipi_dsi_base + MIPIDSI_VID_MODE_CFG_OFFSET, 0x3f, 6, 8);
	/* set_reg(mipi_dsi_base + MIPIDSI_VID_MODE_CFG_OFFSET, 0x0, 1, 14); */
	if (is_mipi_video_panel(hisifd)) {
		set_reg(mipi_dsi_base + MIPIDSI_DPI_LP_CMD_TIM_OFFSET, 0x4, 8, 16);
		set_reg(mipi_dsi_base + MIPIDSI_VID_MODE_CFG_OFFSET, 0x1, 1, 15);
	}

	if ((pinfo->mipi.dsi_version == DSI_1_2_VERSION)
	    && (is_mipi_video_panel(hisifd))
	    && ((pinfo->ifbc_type == IFBC_TYPE_VESA3X_SINGLE)
		 || (pinfo->ifbc_type == IFBC_TYPE_VESA3X_DUAL))) {

		set_reg(mipi_dsi_base + MIPIDSI_VID_PKT_SIZE_OFFSET,
			rect.w * pinfo->pxl_clk_rate_div, 14, 0);

		if (pinfo->mipi.burst_mode < DSI_BURST_SYNC_PULSES_1) {
			HISI_FB_INFO
			    ("pinfo->mipi.burst_mode = %d. video need config BURST mode\n",
			     pinfo->mipi.burst_mode);
			pinfo->mipi.burst_mode = DSI_BURST_SYNC_PULSES_1;
		}
	} else {
		set_reg(mipi_dsi_base + MIPIDSI_VID_PKT_SIZE_OFFSET, rect.w, 14, 0);
	}

	set_reg(mipi_dsi_base + MIPIDSI_VID_MODE_CFG_OFFSET,
		pinfo->mipi.burst_mode, 2, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PCKHDL_CFG_OFFSET, 0x1, 1, 2);

	/*
	 ** 4. Define the DPI Horizontal timing configuration:
	 **
	 ** Hsa_time = HSA*(PCLK period/Clk Lane Byte Period);
	 ** Hbp_time = HBP*(PCLK period/Clk Lane Byte Period);
	 ** Hline_time = (HSA+HBP+HACT+HFP)*(PCLK period/Clk Lane Byte Period);
	 */
	pixel_clk = mipi_pixel_clk(hisifd);
	hsa_time =
	    pinfo->ldi.h_pulse_width * pinfo->dsi_phy_ctrl.lane_byte_clk /
	    pixel_clk;
	hbp_time =
	    pinfo->ldi.h_back_porch * pinfo->dsi_phy_ctrl.lane_byte_clk /
	    pixel_clk;
	hline_time =
	    (pinfo->ldi.h_pulse_width + pinfo->ldi.h_back_porch + rect.w +
	     pinfo->ldi.h_front_porch) * pinfo->dsi_phy_ctrl.lane_byte_clk /
	    pixel_clk;
	set_reg(mipi_dsi_base + MIPIDSI_VID_HSA_TIME_OFFSET, hsa_time, 12, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_HBP_TIME_OFFSET, hbp_time, 12, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_HLINE_TIME_OFFSET, hline_time, 15, 0);

	set_reg(mipi_dsi_base + MIPIDSI_VID_VSA_LINES_OFFSET,
		pinfo->ldi.v_pulse_width, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_VBP_LINES_OFFSET,
		pinfo->ldi.v_back_porch, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_VFP_LINES_OFFSET,
		pinfo->ldi.v_front_porch, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_VID_VACTIVE_LINES_OFFSET, rect.h, 14, 0);
	set_reg(mipi_dsi_base + MIPIDSI_TO_CNT_CFG_OFFSET, 0x7FF, 16, 0);

	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_LPCLK_CFG_OFFSET,
		pinfo->dsi_phy_ctrl.clk_lane_lp2hs_time, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_LPCLK_CFG_OFFSET,
		pinfo->dsi_phy_ctrl.clk_lane_hs2lp_time, 10, 16);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_RD_CFG_OFFSET, 0x7FFF, 15, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_CFG_OFFSET,
		pinfo->dsi_phy_ctrl.data_lane_lp2hs_time, 10, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_TMR_CFG_OFFSET,
		pinfo->dsi_phy_ctrl.data_lane_hs2lp_time, 10, 16);

	set_reg(mipi_dsi_base + MIPIDSI_PWR_UP_OFFSET, 0x1, 1, 0);
}

int mipi_dsi_clk_enable(struct hisi_fb_data_type *hisifd)
{
	int ret = 0;
	struct clk *clk_tmp = NULL;

	BUG_ON(hisifd == NULL);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		clk_tmp = hisifd->dss_dphy0_ref_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy0_ref_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy0_ref_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}

		clk_tmp = hisifd->dss_dphy0_cfg_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy0_cfg_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy0_cfg_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}

		clk_tmp = hisifd->dss_pclk_dsi0_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pclk_dsi0_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pclk_dsi0_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}
	}
#ifdef CONFIG_PCLK_PCTRL_USED
	clk_tmp = hisifd->dss_pclk_pctrl_clk;
	if (clk_tmp) {
		ret = clk_prepare(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_pclk_pctrl_clk clk_prepare failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}

		ret = clk_enable(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_pclk_pctrl_clk clk_enable failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}
	}
#endif

	if (is_dual_mipi_panel(hisifd) || (hisifd->index == EXTERNAL_PANEL_IDX)) {
		clk_tmp = hisifd->dss_dphy1_ref_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy1_ref_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy1_ref_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}

		clk_tmp = hisifd->dss_dphy1_cfg_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy1_cfg_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_dphy1_cfg_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}

		clk_tmp = hisifd->dss_pclk_dsi1_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pclk_dsi1_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pclk_dsi1_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}
	}

	return 0;
}

int mipi_dsi_clk_disable(struct hisi_fb_data_type *hisifd)
{
	struct clk *clk_tmp = NULL;

	BUG_ON(hisifd == NULL);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		clk_tmp = hisifd->dss_dphy0_ref_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}

		clk_tmp = hisifd->dss_dphy0_cfg_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}

		clk_tmp = hisifd->dss_pclk_dsi0_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}
	}
#ifdef CONFIG_PCLK_PCTRL_USED
	clk_tmp = hisifd->dss_pclk_pctrl_clk;
	if (clk_tmp) {
		clk_disable(clk_tmp);
		clk_unprepare(clk_tmp);
	}
#endif

	if (is_dual_mipi_panel(hisifd) || (hisifd->index == EXTERNAL_PANEL_IDX)) {
		clk_tmp = hisifd->dss_dphy1_ref_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}

		clk_tmp = hisifd->dss_dphy1_cfg_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}

		clk_tmp = hisifd->dss_pclk_dsi1_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}
	}

	return 0;
}

/*******************************************************************************
 **
 */
static int mipi_dsi_on_sub1(struct hisi_fb_data_type *hisifd,
			    char __iomem *mipi_dsi_base)
{
	BUG_ON(mipi_dsi_base == NULL);

	/* mipi init */
	mipi_init(hisifd, mipi_dsi_base);

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

static int mipi_dsi_on_sub2(struct hisi_fb_data_type *hisifd,
			    char __iomem *mipi_dsi_base)
{
	struct hisi_panel_info *pinfo = NULL;
	uint32_t pctrl_dphytx_stopcnt = 0;

	BUG_ON(hisifd == NULL);
	BUG_ON(mipi_dsi_base == NULL);

	pinfo = &(hisifd->panel_info);

	if (is_mipi_video_panel(hisifd)) {
		/* switch to video mode */
		set_reg(mipi_dsi_base + MIPIDSI_MODE_CFG_OFFSET, 0x0, 1, 0);
	}

	if (is_mipi_cmd_panel(hisifd)) {
		/* cmd mode: high speed mode */
		set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 7, 8);
		set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 4, 16);
		set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 1, 24);
	}

	/* enable EOTP TX */
	set_reg(mipi_dsi_base + MIPIDSI_PCKHDL_CFG_OFFSET, 0x1, 1, 0);

	/* enable generate High Speed clock, non continue */
	if (pinfo->mipi.non_continue_en) {
		set_reg(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET, 0x3, 2, 0);
	} else {
		set_reg(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET, 0x1, 2, 0);
	}

	if ((pinfo->mipi.dsi_version == DSI_1_2_VERSION)
	    && (pinfo->ifbc_type == IFBC_TYPE_VESA3X_SINGLE)) {
		set_reg(mipi_dsi_base + MIPIDSI_DSC_PARAMETER_OFFSET, 0x01, 32,
			0);
	}

	pctrl_dphytx_stopcnt = (uint64_t) (pinfo->ldi.h_back_porch +
					   pinfo->ldi.h_front_porch +
					   pinfo->ldi.h_pulse_width +
					   5) *
	    hisifd->dss_clk_rate.dss_pclk_pctrl_rate / pinfo->pxl_clk_rate;

	outp32(hisifd->pctrl_base + PERI_CTRL29, pctrl_dphytx_stopcnt);
	if (is_dual_mipi_panel(hisifd)) {
		outp32(hisifd->pctrl_base + PERI_CTRL32, pctrl_dphytx_stopcnt);
	}

	return 0;
}

int mipi_dsi_off_sub(struct hisi_fb_data_type *hisifd,
		     char __iomem *mipi_dsi_base)
{
	BUG_ON(hisifd == NULL);
	BUG_ON(mipi_dsi_base == NULL);

	/* switch to cmd mode */
	set_reg(mipi_dsi_base + MIPIDSI_MODE_CFG_OFFSET, 0x1, 1, 0);
	/* cmd mode: low power mode */
	set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x7f, 7, 8);
	set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0xf, 4, 16);
	set_reg(mipi_dsi_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x1, 1, 24);

	/* disable generate High Speed clock */
	set_reg(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET, 0x0, 1, 0);

	/* shutdown d_phy */
	set_reg(mipi_dsi_base + MIPIDSI_PHY_RSTZ_OFFSET, 0x0, 3, 0);

	return 0;
}

static int mipi_dsi_ulps_enter(struct hisi_fb_data_type *hisifd,
			       char __iomem *mipi_dsi_base)
{
	uint32_t tmp = 0;
	uint32_t cmp_ulpsactivenot_val = 0;
	uint32_t cmp_stopstate_val = 0;
	uint32_t try_times = 0;

	BUG_ON(hisifd == NULL);
	BUG_ON(mipi_dsi_base == NULL);

	HISI_FB_DEBUG("fb%d, +!\n", hisifd->index);

	if (hisifd->panel_info.mipi.lane_nums >= DSI_4_LANES) {
		cmp_ulpsactivenot_val = (BIT(5) | BIT(8) | BIT(10) | BIT(12));
		cmp_stopstate_val = (BIT(4) | BIT(7) | BIT(9) | BIT(11));
	} else if (hisifd->panel_info.mipi.lane_nums >= DSI_3_LANES) {
		cmp_ulpsactivenot_val = (BIT(5) | BIT(8) | BIT(10));
		cmp_stopstate_val = (BIT(4) | BIT(7) | BIT(9));
	} else if (hisifd->panel_info.mipi.lane_nums >= DSI_2_LANES) {
		cmp_ulpsactivenot_val = (BIT(5) | BIT(8));
		cmp_stopstate_val = (BIT(4) | BIT(7));
	} else {
		cmp_ulpsactivenot_val = (BIT(5));
		cmp_stopstate_val = (BIT(4));
	}

	if (inp32(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET) & (BIT(1)))
		cmp_stopstate_val |= (BIT(2));

	try_times = 0;
	tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	while ((tmp & cmp_stopstate_val) != cmp_stopstate_val) {
		udelay(10);
		if (++try_times > 100) {
			HISI_FB_ERR
			    ("fb%d, check DPHY data and clock lane stopstate failed! MIPIDSI_PHY_STATUS=0x%x.\n",
			     hisifd->index, tmp);
			return 0;
		}

		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	}

	if (mipi_dsi_base == hisifd->mipi_dsi0_base) {
		set_reg(hisifd->pctrl_base + PERI_CTRL23, 0x0, 1, 3);
	} else {
		set_reg(hisifd->pctrl_base + PERI_CTRL23, 0x0, 1, 4);
	}
	set_reg(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET, 0x0, 1, 0);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_ULPS_CTRL_OFFSET, 0x4, 4, 0);

	try_times = 0;
	tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	while ((tmp & cmp_ulpsactivenot_val) != 0) {
		udelay(10);
		if (++try_times > 100) {
			HISI_FB_ERR
			    ("fb%d, check DPHY data lane ulpsactivenot_status failed! MIPIDSI_PHY_STATUS=0x%x.\n",
			     hisifd->index, tmp);
			break;
		}
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	}
	set_reg(mipi_dsi_base + MIPIDSI_PHY_ULPS_CTRL_OFFSET, 0x5, 4, 0);

	try_times = 0;
	tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	while ((tmp & BIT(3)) != 0) {
		udelay(10);
		if (++try_times > 100) {
			HISI_FB_ERR
			    ("fb%d, check DPHY clock lane ulpsactivenot_status failed! MIPIDSI_PHY_STATUS=0x%x.\n",
			     hisifd->index, tmp);
			break;
		}
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	}
	outp32(mipi_dsi_base + MIPIDSI_PHY_RSTZ_OFFSET, 0x7);

	try_times = 0;
	tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	while ((tmp & BIT(0)) != 0) {
		udelay(10);
		if (++try_times > 100) {
			HISI_FB_ERR
			    ("fb%d, check DPHY clock lane phy_lock failed! MIPIDSI_PHY_STATUS=0x%x.\n",
			     hisifd->index, tmp);
			break;
		}
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	}

	set_reg(hisifd->peri_crg_base + PERDIS3, 0x1, 4, 28);
	HISI_FB_DEBUG("fb%d, -!\n", hisifd->index);

	return 0;
}

static int mipi_dsi_ulps_exit(struct hisi_fb_data_type *hisifd,
			      char __iomem *mipi_dsi_base)
{
	uint32_t tmp = 0;
	uint32_t cmp_ulpsactivenot_val = 0;
	uint32_t try_times = 0;

	BUG_ON(hisifd == NULL);
	BUG_ON(mipi_dsi_base == NULL);

	HISI_FB_DEBUG("fb%d, +!\n", hisifd->index);

	set_reg(hisifd->peri_crg_base + PEREN3, 0x1, 4, 28);
	set_reg(mipi_dsi_base + MIPIDSI_PHY_RSTZ_OFFSET, 0x1, 1, 3);

	try_times = 0;
	tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	while ((tmp & BIT(0)) != 1) {
		udelay(10);
		if (++try_times > 100) {
			HISI_FB_ERR
			    ("fb%d, check DPHY clock lane phy_lock failed! MIPIDSI_PHY_STATUS=0x%x.\n",
			     hisifd->index, tmp);
			break;
		}
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	}

	if (hisifd->panel_info.mipi.lane_nums >= DSI_4_LANES) {
		cmp_ulpsactivenot_val =
		    (BIT(3) | BIT(5) | BIT(8) | BIT(10) | BIT(12));
	} else if (hisifd->panel_info.mipi.lane_nums >= DSI_3_LANES) {
		cmp_ulpsactivenot_val = (BIT(3) | BIT(5) | BIT(8) | BIT(10));
	} else if (hisifd->panel_info.mipi.lane_nums >= DSI_2_LANES) {
		cmp_ulpsactivenot_val = (BIT(3) | BIT(5) | BIT(8));
	} else {
		cmp_ulpsactivenot_val = (BIT(3) | BIT(5));
	}

	if (mipi_dsi_base == hisifd->mipi_dsi0_base) {
		set_reg(hisifd->pctrl_base + PERI_CTRL23, 0x1, 1, 3);
	} else {
		set_reg(hisifd->pctrl_base + PERI_CTRL23, 0x1, 1, 4);
	}

	outp32(mipi_dsi_base + MIPIDSI_PHY_ULPS_CTRL_OFFSET, 0xF);
	try_times = 0;
	tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	while ((tmp & cmp_ulpsactivenot_val) != cmp_ulpsactivenot_val) {
		udelay(10);
		if (++try_times > 100) {
			HISI_FB_ERR
			    ("fb%d, failed to request that data lane and clock lane exit ULPS!MIPIDSI_PHY_STATUS=0x%x.\n",
			     hisifd->index, tmp);
			break;
		}
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	}
	mdelay(1);
	outp32(mipi_dsi_base + MIPIDSI_PHY_ULPS_CTRL_OFFSET, 0x0);

	try_times = 0;
	tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	while ((tmp & BIT(0)) != 0x1) {
		udelay(10);
		if (++try_times > 100) {
			HISI_FB_ERR
			    ("fb%d, failed to wait DPHY PLL Lock!MIPIDSI_PHY_STATUS=0x%x.\n",
			     hisifd->index, tmp);
			break;
		}
		tmp = inp32(mipi_dsi_base + MIPIDSI_PHY_STATUS_OFFSET);
	}

	set_reg(mipi_dsi_base + MIPIDSI_LPCLK_CTRL_OFFSET, 0x1, 1, 0);
	HISI_FB_DEBUG("fb%d, -!\n", hisifd->index);

	return 0;
}

int mipi_dsi_ulps_cfg(struct hisi_fb_data_type *hisifd, int enable)
{
	int ret = 0;

	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	if (enable) {
		mipi_dsi_ulps_exit(hisifd, hisifd->mipi_dsi0_base);
		if (is_dual_mipi_panel(hisifd))
			mipi_dsi_ulps_exit(hisifd, hisifd->mipi_dsi1_base);
	} else {
		mipi_dsi_ulps_enter(hisifd, hisifd->mipi_dsi0_base);
		if (is_dual_mipi_panel(hisifd))
			mipi_dsi_ulps_enter(hisifd, hisifd->mipi_dsi1_base);
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

void mipi_dsi_reset(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);
	set_reg(hisifd->mipi_dsi0_base + MIPIDSI_PWR_UP_OFFSET, 0x0, 1, 0);
	msleep(2);
	set_reg(hisifd->mipi_dsi0_base + MIPIDSI_PWR_UP_OFFSET, 0x1, 1, 0);
}

/*******************************************************************************
 **
 */
static int mipi_dsi_on(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	/* set LCD init step before LCD on */
	hisifd->panel_info.lcd_init_step = LCD_INIT_POWER_ON;
	ret = panel_next_on(pdev);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		if (is_dual_mipi_panel(hisifd))
			outp32(hisifd->peri_crg_base + PERRSTDIS3, 0x30000000);
		else
			outp32(hisifd->peri_crg_base + PERRSTDIS3, 0x10000000);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		outp32(hisifd->peri_crg_base + PERRSTDIS3, 0x20000000);
	} else {
		HISI_FB_ERR("fb%d, not supported!\n", hisifd->index);
	}

	mipi_dsi_clk_enable(hisifd);
	if (hisifd->index == PRIMARY_PANEL_IDX) {
		mipi_dsi_on_sub1(hisifd, hisifd->mipi_dsi0_base);
		if (is_dual_mipi_panel(hisifd))
			mipi_dsi_on_sub1(hisifd, hisifd->mipi_dsi1_base);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		mipi_dsi_on_sub1(hisifd, hisifd->mipi_dsi1_base);
	} else {
		HISI_FB_ERR("fb%d, not supported!\n", hisifd->index);
	}

	ret = panel_next_on(pdev);
	if (hisifd->index == PRIMARY_PANEL_IDX) {
		mipi_dsi_on_sub2(hisifd, hisifd->mipi_dsi0_base);
		if (is_dual_mipi_panel(hisifd))
			mipi_dsi_on_sub2(hisifd, hisifd->mipi_dsi1_base);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		mipi_dsi_on_sub2(hisifd, hisifd->mipi_dsi1_base);
	} else {
		HISI_FB_ERR("fb%d, not supported!\n", hisifd->index);
	}

	/* mipi hs video/command mode */
	ret = panel_next_on(pdev);

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int mipi_dsi_off(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	/* set LCD uninit step before LCD off */
	hisifd->panel_info.lcd_uninit_step = LCD_UNINIT_MIPI_HS_SEND_SEQUENCE;
	ret = panel_next_off(pdev);

	if (hisifd->panel_info.lcd_uninit_step_support) {
		/* TODO: add MIPI LP mode here if necessary */
		/* MIPI LP mode end */
		ret = panel_next_off(pdev);
	}

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		mipi_dsi_off_sub(hisifd, hisifd->mipi_dsi0_base);
		if (is_dual_mipi_panel(hisifd))
			mipi_dsi_off_sub(hisifd, hisifd->mipi_dsi1_base);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		mipi_dsi_off_sub(hisifd, hisifd->mipi_dsi1_base);
	} else {
		HISI_FB_ERR("fb%d, not supported!\n", hisifd->index);
	}

	mipi_dsi_clk_disable(hisifd);
	if (hisifd->index == PRIMARY_PANEL_IDX) {
		if (is_dual_mipi_panel(hisifd))
			outp32(hisifd->peri_crg_base + PERRSTEN3, 0x30000000);
		else
			outp32(hisifd->peri_crg_base + PERRSTEN3, 0x10000000);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		outp32(hisifd->peri_crg_base + PERRSTEN3, 0x20000000);
	} else {
		HISI_FB_ERR("fb%d, not supported!\n", hisifd->index);
	}

	if (hisifd->panel_info.lcd_uninit_step_support) {
		ret = panel_next_off(pdev);
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int mipi_dsi_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = panel_next_remove(pdev);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		if (hisifd->dss_dphy0_ref_clk) {
			clk_put(hisifd->dss_dphy0_ref_clk);
			hisifd->dss_dphy0_ref_clk = NULL;
		}

		if (hisifd->dss_dphy0_cfg_clk) {
			clk_put(hisifd->dss_dphy0_cfg_clk);
			hisifd->dss_dphy0_cfg_clk = NULL;
		}

		if (is_dual_mipi_panel(hisifd)) {
			if (hisifd->dss_dphy1_ref_clk) {
				clk_put(hisifd->dss_dphy1_ref_clk);
				hisifd->dss_dphy1_ref_clk = NULL;
			}

			if (hisifd->dss_dphy1_cfg_clk) {
				clk_put(hisifd->dss_dphy1_cfg_clk);
				hisifd->dss_dphy1_cfg_clk = NULL;
			}
		}
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		if (hisifd->dss_dphy1_ref_clk) {
			clk_put(hisifd->dss_dphy1_ref_clk);
			hisifd->dss_dphy1_ref_clk = NULL;
		}

		if (hisifd->dss_dphy1_cfg_clk) {
			clk_put(hisifd->dss_dphy1_cfg_clk);
			hisifd->dss_dphy1_cfg_clk = NULL;
		}
	} else {
		HISI_FB_ERR("fb%d, not supported!\n", hisifd->index);
	}
	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int mipi_dsi_set_backlight(struct platform_device *pdev,
				  uint32_t bl_level)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = panel_next_set_backlight(pdev, bl_level);

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int mipi_dsi_vsync_ctrl(struct platform_device *pdev, int enable)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = panel_next_vsync_ctrl(pdev, enable);

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int mipi_dsi_clk_irq_setup(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	int ret = 0;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		ret =
		    clk_set_rate(hisifd->dss_dphy0_ref_clk,
				 DEFAULT_MIPI_CLK_RATE);
		if (ret < 0) {
			HISI_FB_ERR
			    ("fb%d dss_dphy0_ref_clk clk_set_rate(%lu) failed, error=%d!\n",
			     hisifd->index, DEFAULT_MIPI_CLK_RATE, ret);
			return -EINVAL;
		}
		HISI_FB_INFO("dss_dphy0_ref_clk:[%lu]->[%lu].\n",
			     DEFAULT_MIPI_CLK_RATE,
			     clk_get_rate(hisifd->dss_dphy0_ref_clk));

		ret =
		    clk_set_rate(hisifd->dss_dphy0_cfg_clk,
				 DEFAULT_MIPI_CLK_RATE);
		if (ret < 0) {
			HISI_FB_ERR
			    ("fb%d dss_dphy0_cfg_clk clk_set_rate(%lu) failed, error=%d!\n",
			     hisifd->index, DEFAULT_MIPI_CLK_RATE, ret);
			return -EINVAL;
		}
		HISI_FB_INFO("dss_dphy0_cfg_clk:[%lu]->[%lu].\n",
			     DEFAULT_MIPI_CLK_RATE,
			     clk_get_rate(hisifd->dss_dphy0_cfg_clk));
		HISI_FB_INFO("dss_pclk_dsi0_clk:[%lu]->[%lu].\n",
			     DEFAULT_PCLK_DSI_RATE,
			     clk_get_rate(hisifd->dss_pclk_dsi0_clk));
	}
#ifdef CONFIG_PCLK_PCTRL_USED
	ret = clk_set_rate(hisifd->dss_pclk_pctrl_clk, DEFAULT_PCLK_PCTRL_RATE);
	if (ret < 0) {
		HISI_FB_ERR
		    ("fb%d dss_pclk_pctrl clk_set_rate(%lu) failed, error=%d!\n",
		     hisifd->index, DEFAULT_PCLK_PCTRL_RATE, ret);
		return -EINVAL;
	}
	HISI_FB_INFO("dss_pclk_pctrl_clk:[%lu]->[%lu].\n",
		     DEFAULT_PCLK_PCTRL_RATE,
		     clk_get_rate(hisifd->dss_pclk_pctrl_clk));
#endif

	if (is_dual_mipi_panel(hisifd) || (hisifd->index == EXTERNAL_PANEL_IDX)) {
		ret =
		    clk_set_rate(hisifd->dss_dphy1_ref_clk,
				 DEFAULT_MIPI_CLK_RATE);
		if (ret < 0) {
			HISI_FB_ERR
			    ("fb%d dss_dphy1_ref_clk clk_set_rate(%lu) failed, error=%d!\n",
			     hisifd->index, DEFAULT_MIPI_CLK_RATE, ret);
			return -EINVAL;
		}
		HISI_FB_INFO("dss_dphy1_ref_clk:[%lu]->[%lu].\n",
			     DEFAULT_MIPI_CLK_RATE,
			     clk_get_rate(hisifd->dss_dphy1_ref_clk));

		ret =
		    clk_set_rate(hisifd->dss_dphy1_cfg_clk,
				 DEFAULT_MIPI_CLK_RATE);
		if (ret < 0) {
			HISI_FB_ERR
			    ("fb%d dss_dphy1_cfg_clk clk_set_rate(%lu) failed, "
			     "error=%d!\n",
			     hisifd->index, DEFAULT_MIPI_CLK_RATE, ret);
			return -EINVAL;
		}
		HISI_FB_INFO("dss_dphy1_cfg_clk:[%lu]->[%lu].\n",
			     DEFAULT_MIPI_CLK_RATE,
			     clk_get_rate(hisifd->dss_dphy1_cfg_clk));
		HISI_FB_INFO("dss_pclk_dsi1_clk:[%lu]->[%lu].\n",
			     DEFAULT_PCLK_DSI_RATE,
			     clk_get_rate(hisifd->dss_pclk_dsi1_clk));
	}

	return ret;
}

static int mipi_dsi_probe(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct platform_device *dpp_dev = NULL;
	struct hisi_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = mipi_dsi_clk_irq_setup(pdev);
	if (ret) {
		HISI_FB_ERR("fb%d mipi_dsi_irq_clk_setup failed, error=%d!\n",
			    hisifd->index, ret);
		goto err;
	}
	/* alloc device */
	dpp_dev = platform_device_alloc(DEV_NAME_DSS_DPE, pdev->id);
	if (!dpp_dev) {
		HISI_FB_ERR("fb%d platform_device_alloc failed, error=%d!\n",
			    hisifd->index, ret);
		ret = -ENOMEM;
		goto err_device_alloc;
	}
	/* link to the latest pdev */
	hisifd->pdev = dpp_dev;

	/* alloc panel device data */
	ret = platform_device_add_data(dpp_dev, dev_get_platdata(&pdev->dev),
				       sizeof(struct hisi_fb_panel_data));
	if (ret) {
		HISI_FB_ERR("fb%d platform_device_add_data failed error=%d!\n",
			    hisifd->index, ret);
		goto err_device_put;
	}

	/* data chain */
	pdata = dev_get_platdata(&dpp_dev->dev);
	pdata->on = mipi_dsi_on;
	pdata->off = mipi_dsi_off;
	pdata->remove = mipi_dsi_remove;
	pdata->set_backlight = mipi_dsi_set_backlight;
	pdata->vsync_ctrl = mipi_dsi_vsync_ctrl;
	pdata->next = pdev;

	/* get/set panel info */
	memcpy(&hisifd->panel_info, pdata->panel_info,
	       sizeof(struct hisi_panel_info));

	/* set driver data */
	platform_set_drvdata(dpp_dev, hisifd);
	/* device add */
	ret = platform_device_add(dpp_dev);
	if (ret) {
		HISI_FB_ERR("fb%d platform_device_add failed, error=%d!\n",
			    hisifd->index, ret);
		goto err_device_put;
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return 0;

 err_device_put:
	platform_device_put(dpp_dev);
 err_device_alloc:
 err:
	return ret;
}

static struct platform_driver this_driver = {
	.probe = mipi_dsi_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = DEV_NAME_MIPIDSI,
		   },
};

static int __init mipi_dsi_driver_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		HISI_FB_ERR("platform_driver_register failed, error=%d!\n",
			    ret);
		return ret;
	}
	return ret;
}

module_init(mipi_dsi_driver_init);

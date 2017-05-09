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
#include <drm/drmP.h>

#include "drm_mipi_dsi.h"
#include "kirin_drm_dpe_utils.h"

int g_debug_set_reg_val = 0;

extern u32 g_dss_module_ovl_base[DSS_MCTL_IDX_MAX][MODULE_OVL_MAX];

mipi_ifbc_division_t g_mipi_ifbc_division[MIPI_DPHY_NUM][IFBC_TYPE_MAX] = {
	/*single mipi*/
	{
		/*none*/
		{XRES_DIV_1, YRES_DIV_1, IFBC_COMP_MODE_0, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_0, PXL0_DSI_GT_EN_1},
		/*orise2x*/
		{XRES_DIV_2, YRES_DIV_1, IFBC_COMP_MODE_0, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_1, PXL0_DSI_GT_EN_3},
		/*orise3x*/
		{XRES_DIV_3, YRES_DIV_1, IFBC_COMP_MODE_1, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_2, PXL0_DSI_GT_EN_3},
		/*himax2x*/
		{XRES_DIV_2, YRES_DIV_1, IFBC_COMP_MODE_2, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_1, PXL0_DSI_GT_EN_3},
		/*rsp2x*/
		{XRES_DIV_2, YRES_DIV_1, IFBC_COMP_MODE_3, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_OPEN, PXL0_DIVCFG_1, PXL0_DSI_GT_EN_3},
		/*rsp3x  [NOTE]reality: xres_div = 1.5, yres_div = 2, amended in "mipi_ifbc_get_rect" function*/
		{XRES_DIV_3, YRES_DIV_1, IFBC_COMP_MODE_4, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_OPEN, PXL0_DIVCFG_2, PXL0_DSI_GT_EN_3},
		/*vesa2x_1pipe*/
		{XRES_DIV_2, YRES_DIV_1, IFBC_COMP_MODE_5, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_1, PXL0_DSI_GT_EN_3},
		/*vesa3x_1pipe*/
		{XRES_DIV_3, YRES_DIV_1, IFBC_COMP_MODE_5, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_2, PXL0_DSI_GT_EN_3},
		/*vesa2x_2pipe*/
		{XRES_DIV_2, YRES_DIV_1, IFBC_COMP_MODE_6, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_1, PXL0_DSI_GT_EN_3},
		/*vesa3x_2pipe*/
		{XRES_DIV_3, YRES_DIV_1, IFBC_COMP_MODE_6, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_2, PXL0_DSI_GT_EN_3}
	},

	/*dual mipi*/
	{
		/*none*/
		{XRES_DIV_2, YRES_DIV_1, IFBC_COMP_MODE_0, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_1, PXL0_DSI_GT_EN_3},
		/*orise2x*/
		{XRES_DIV_4, YRES_DIV_1, IFBC_COMP_MODE_0, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_3, PXL0_DSI_GT_EN_3},
		/*orise3x*/
		{XRES_DIV_6, YRES_DIV_1, IFBC_COMP_MODE_1, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_5, PXL0_DSI_GT_EN_3},
		/*himax2x*/
		{XRES_DIV_4, YRES_DIV_1, IFBC_COMP_MODE_2, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_3, PXL0_DSI_GT_EN_3},
		/*rsp2x*/
		{XRES_DIV_4, YRES_DIV_1, IFBC_COMP_MODE_3, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_OPEN, PXL0_DIVCFG_3, PXL0_DSI_GT_EN_3},
		/*rsp3x*/
		{XRES_DIV_3, YRES_DIV_2, IFBC_COMP_MODE_4, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_OPEN, PXL0_DIVCFG_5, PXL0_DSI_GT_EN_3},
		/*vesa2x_1pipe*/
		{XRES_DIV_4, YRES_DIV_1, IFBC_COMP_MODE_5, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_3, PXL0_DSI_GT_EN_3},
		/*vesa3x_1pipe*/
		{XRES_DIV_6, YRES_DIV_1, IFBC_COMP_MODE_5, PXL0_DIV2_GT_EN_CLOSE,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_5, PXL0_DSI_GT_EN_3},
		/*vesa2x_2pipe*/
		{XRES_DIV_4, YRES_DIV_1, IFBC_COMP_MODE_6, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_3, PXL0_DSI_GT_EN_3},
		/*vesa3x_2pipe*/
		{XRES_DIV_6, YRES_DIV_1, IFBC_COMP_MODE_6, PXL0_DIV2_GT_EN_OPEN,
			PXL0_DIV4_GT_EN_CLOSE, PXL0_DIVCFG_5, 3} }
};

void set_reg(char __iomem *addr, uint32_t val, uint8_t bw, uint8_t bs)
{
	u32 mask = (1UL << bw) - 1UL;
	u32 tmp = 0;

	tmp = inp32(addr);
	tmp &= ~(mask << bs);

	outp32(addr, tmp | ((val & mask) << bs));

	if (g_debug_set_reg_val) {
		printk(KERN_INFO "writel: [%p] = 0x%x\n", addr,
			     tmp | ((val & mask) << bs));
	}
}

static int mipi_ifbc_get_rect(struct dss_rect *rect)
{
	u32 ifbc_type;
	u32 mipi_idx;
	u32 xres_div;
	u32 yres_div;

	ifbc_type = IFBC_TYPE_NONE;
	mipi_idx = 0;

	xres_div = g_mipi_ifbc_division[mipi_idx][ifbc_type].xres_div;
	yres_div = g_mipi_ifbc_division[mipi_idx][ifbc_type].yres_div;

	if ((rect->w % xres_div) > 0)
		DRM_ERROR("xres(%d) is not division_h(%d) pixel aligned!\n", rect->w, xres_div);

	if ((rect->h % yres_div) > 0)
		DRM_ERROR("yres(%d) is not division_v(%d) pixel aligned!\n", rect->h, yres_div);

	/*
	** [NOTE] rsp3x && single_mipi CMD mode amended xres_div = 1.5, yres_div = 2 ,
	** VIDEO mode amended xres_div = 3, yres_div = 1
	*/
	rect->w /= xres_div;
	rect->h /= yres_div;

	return 0;
}

static void init_ldi_pxl_div(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *ldi_base;
	struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;

	u32 ifbc_type = 0;
	u32 mipi_idx = 0;
	u32 pxl0_div2_gt_en = 0;
	u32 pxl0_div4_gt_en = 0;
	u32 pxl0_divxcfg = 0;
	u32 pxl0_dsi_gt_en = 0;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	mode = &acrtc->base.state->mode;
	adj_mode = &acrtc->base.state->adjusted_mode;

	ldi_base = ctx->base + DSS_LDI0_OFFSET;

	ifbc_type = IFBC_TYPE_NONE;
	mipi_idx = 0;

	pxl0_div2_gt_en = g_mipi_ifbc_division[mipi_idx][ifbc_type].pxl0_div2_gt_en;
	pxl0_div4_gt_en = g_mipi_ifbc_division[mipi_idx][ifbc_type].pxl0_div4_gt_en;
	pxl0_divxcfg = g_mipi_ifbc_division[mipi_idx][ifbc_type].pxl0_divxcfg;
	pxl0_dsi_gt_en = g_mipi_ifbc_division[mipi_idx][ifbc_type].pxl0_dsi_gt_en;

	set_reg(ldi_base + LDI_PXL0_DIV2_GT_EN, pxl0_div2_gt_en, 1, 0);
	set_reg(ldi_base + LDI_PXL0_DIV4_GT_EN, pxl0_div4_gt_en, 1, 0);
	set_reg(ldi_base + LDI_PXL0_GT_EN, 0x1, 1, 0);
	set_reg(ldi_base + LDI_PXL0_DSI_GT_EN, pxl0_dsi_gt_en, 2, 0);
	set_reg(ldi_base + LDI_PXL0_DIVXCFG, pxl0_divxcfg, 3, 0);
}

void init_other(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *dss_base;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	dss_base = ctx->base;

	/**
	 * VESA_CLK_SEL is set to 0 for initial,
	 * 1 is needed only by vesa dual pipe compress
	 */
	set_reg(dss_base + DSS_LDI0_OFFSET + LDI_VESA_CLK_SEL, 0, 1, 0);
}

void init_ldi(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *ldi_base;
	struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;

	dss_rect_t rect = {0, 0, 0, 0};
	u32 hfp, hbp, hsw, vfp, vbp, vsw;
	u32 vsync_plr = 0;
	u32 hsync_plr = 0;
	u32 pixelclk_plr = 0;
	u32 data_en_plr = 0;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return ;
	}

	mode = &acrtc->base.state->mode;
	adj_mode = &acrtc->base.state->adjusted_mode;

	hfp = mode->hsync_start - mode->hdisplay;
	hbp = mode->htotal - mode->hsync_end;
	hsw = mode->hsync_end - mode->hsync_start;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;
	vsw = mode->vsync_end - mode->vsync_start;

	ldi_base = ctx->base + DSS_LDI0_OFFSET;

	rect.x = 0;
	rect.y = 0;
	rect.w = mode->hdisplay;
	rect.h = mode->vdisplay;
	mipi_ifbc_get_rect(&rect);

	init_ldi_pxl_div(acrtc);

	outp32(ldi_base + LDI_DPI0_HRZ_CTRL0,
		hfp | ((hbp + DSS_WIDTH(hsw)) << 16));
	outp32(ldi_base + LDI_DPI0_HRZ_CTRL1, 0);
	outp32(ldi_base + LDI_DPI0_HRZ_CTRL2, DSS_WIDTH(rect.w));
	outp32(ldi_base + LDI_VRT_CTRL0,
		vfp | (vbp << 16));
	outp32(ldi_base + LDI_VRT_CTRL1, DSS_HEIGHT(vsw));
	outp32(ldi_base + LDI_VRT_CTRL2, DSS_HEIGHT(rect.h));

	outp32(ldi_base + LDI_PLR_CTRL,
		vsync_plr | (hsync_plr << 1) |
		(pixelclk_plr << 2) | (data_en_plr << 3));

	/* bpp*/
	set_reg(ldi_base + LDI_CTRL, acrtc->out_format, 2, 3);
	/* bgr*/
	set_reg(ldi_base + LDI_CTRL, acrtc->bgr_fmt, 1, 13);

	/* for ddr pmqos*/
	outp32(ldi_base + LDI_VINACT_MSK_LEN, vfp);

	/*cmd event sel*/
	outp32(ldi_base + LDI_CMD_EVENT_SEL, 0x1);

	/* for 1Hz LCD and mipi command LCD*/
	set_reg(ldi_base + LDI_DSI_CMD_MOD_CTRL, 0x1, 1, 1);

	/*ldi_data_gate(hisifd, true);*/

#ifdef CONFIG_HISI_FB_LDI_COLORBAR_USED
	/* colorbar width*/
	set_reg(ldi_base + LDI_CTRL, DSS_WIDTH(0x3c), 7, 6);
	/* colorbar ort*/
	set_reg(ldi_base + LDI_WORK_MODE, 0x0, 1, 1);
	/* colorbar enable*/
	set_reg(ldi_base + LDI_WORK_MODE, 0x0, 1, 0);
#else
	/* normal*/
	set_reg(ldi_base + LDI_WORK_MODE, 0x1, 1, 0);
#endif

	/* ldi disable*/
	set_reg(ldi_base + LDI_CTRL, 0x0, 1, 0);
}

void init_dbuf(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;
	char __iomem *dbuf_base;

	int sram_valid_num = 0;
	int sram_max_mem_depth = 0;
	int sram_min_support_depth = 0;

	u32 thd_rqos_in = 0;
	u32 thd_rqos_out = 0;
	u32 thd_wqos_in = 0;
	u32 thd_wqos_out = 0;
	u32 thd_cg_in = 0;
	u32 thd_cg_out = 0;
	u32 thd_wr_wait = 0;
	u32 thd_cg_hold = 0;
	u32 thd_flux_req_befdfs_in = 0;
	u32 thd_flux_req_befdfs_out = 0;
	u32 thd_flux_req_aftdfs_in = 0;
	u32 thd_flux_req_aftdfs_out = 0;
	u32 thd_dfs_ok = 0;
	u32 dfs_ok_mask = 0;
	u32 thd_flux_req_sw_en = 1;
	u32 hfp, hbp, hsw, vfp, vbp, vsw;

	int dfs_time = 0;
	int dfs_time_min = 0;
	int depth = 0;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	mode = &acrtc->base.state->mode;
	adj_mode = &acrtc->base.state->adjusted_mode;

	hfp = mode->hsync_start - mode->hdisplay;
	hbp = mode->htotal - mode->hsync_end;
	hsw = mode->hsync_end - mode->hsync_start;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;
	vsw = mode->vsync_end - mode->vsync_start;

	dbuf_base = ctx->base + DSS_DBUF0_OFFSET;

	if (mode->hdisplay * mode->vdisplay >= RES_4K_PHONE)
		dfs_time_min = DFS_TIME_MIN_4K;
	else
		dfs_time_min = DFS_TIME_MIN;

	dfs_time = DFS_TIME;
	depth = DBUF0_DEPTH;

	DRM_DEBUG("dfs_time=%d,\n"
		"adj_mode->clock=%d\n"
		"hsw=%d\n"
		"hbp=%d\n"
		"hfp=%d\n"
		"mode->hdisplay=%d\n"
		"mode->vdisplay=%d\n",
		dfs_time,
		adj_mode->clock,
		hsw,
		hbp,
		hfp,
		mode->hdisplay,
		mode->vdisplay);

	/*
	** int K = 0;
	** int Tp = 1000000  / adj_mode->clock;
	** K = (hsw + hbp + mode->hdisplay +
	**	hfp) / mode->hdisplay;
	** thd_cg_out = dfs_time / (Tp * K * 6);
	*/
	thd_cg_out = (dfs_time * adj_mode->clock * 1000UL * mode->hdisplay) /
		(((hsw + hbp + hfp) + mode->hdisplay) * 6 * 1000000UL);

	sram_valid_num = thd_cg_out / depth;
	thd_cg_in = (sram_valid_num + 1) * depth - 1;

	sram_max_mem_depth = (sram_valid_num + 1) * depth;

	thd_rqos_in = thd_cg_out * 85 / 100;
	thd_rqos_out = thd_cg_out;
	thd_flux_req_befdfs_in = GET_FLUX_REQ_IN(sram_max_mem_depth);
	thd_flux_req_befdfs_out = GET_FLUX_REQ_OUT(sram_max_mem_depth);

	sram_min_support_depth = dfs_time_min * mode->hdisplay / (1000000 / 60 / (mode->vdisplay +
		vbp + vfp + vsw) * (DBUF_WIDTH_BIT / 3 / BITS_PER_BYTE));

	/*thd_flux_req_aftdfs_in   =[(sram_valid_num+1)*depth - 50*HSIZE/((1000000/60/(VSIZE+VFP+VBP+VSW))*6)]/3*/
	thd_flux_req_aftdfs_in = (sram_max_mem_depth - sram_min_support_depth) / 3;
	/*thd_flux_req_aftdfs_out  =  2*[(sram_valid_num+1)* depth - 50*HSIZE/((1000000/60/(VSIZE+VFP+VBP+VSW))*6)]/3*/
	thd_flux_req_aftdfs_out = 2 * (sram_max_mem_depth - sram_min_support_depth) / 3;

	thd_dfs_ok = thd_flux_req_befdfs_in;

	DRM_DEBUG("hdisplay=%d\n"
		"vdisplay=%d\n"
		"sram_valid_num=%d,\n"
		"thd_rqos_in=0x%x\n"
		"thd_rqos_out=0x%x\n"
		"thd_cg_in=0x%x\n"
		"thd_cg_out=0x%x\n"
		"thd_flux_req_befdfs_in=0x%x\n"
		"thd_flux_req_befdfs_out=0x%x\n"
		"thd_flux_req_aftdfs_in=0x%x\n"
		"thd_flux_req_aftdfs_out=0x%x\n"
		"thd_dfs_ok=0x%x\n",
		mode->hdisplay,
		mode->vdisplay,
		sram_valid_num,
		thd_rqos_in,
		thd_rqos_out,
		thd_cg_in,
		thd_cg_out,
		thd_flux_req_befdfs_in,
		thd_flux_req_befdfs_out,
		thd_flux_req_aftdfs_in,
		thd_flux_req_aftdfs_out,
		thd_dfs_ok);

	outp32(dbuf_base + DBUF_FRM_SIZE, mode->hdisplay * mode->vdisplay);
	outp32(dbuf_base + DBUF_FRM_HSIZE, DSS_WIDTH(mode->hdisplay));
	outp32(dbuf_base + DBUF_SRAM_VALID_NUM, sram_valid_num);

	outp32(dbuf_base + DBUF_THD_RQOS, (thd_rqos_out << 16) | thd_rqos_in);
	outp32(dbuf_base + DBUF_THD_WQOS, (thd_wqos_out << 16) | thd_wqos_in);
	outp32(dbuf_base + DBUF_THD_CG, (thd_cg_out << 16) | thd_cg_in);
	outp32(dbuf_base + DBUF_THD_OTHER, (thd_cg_hold << 16) | thd_wr_wait);
	outp32(dbuf_base + DBUF_THD_FLUX_REQ_BEF, (thd_flux_req_befdfs_out << 16) | thd_flux_req_befdfs_in);
	outp32(dbuf_base + DBUF_THD_FLUX_REQ_AFT, (thd_flux_req_aftdfs_out << 16) | thd_flux_req_aftdfs_in);
	outp32(dbuf_base + DBUF_THD_DFS_OK, thd_dfs_ok);
	outp32(dbuf_base + DBUF_FLUX_REQ_CTRL, (dfs_ok_mask << 1) | thd_flux_req_sw_en);

	outp32(dbuf_base + DBUF_DFS_LP_CTRL, 0x1);
}

void init_dpp(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;
	char __iomem *dpp_base;
	char __iomem *mctl_sys_base;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	mode = &acrtc->base.state->mode;
	adj_mode = &acrtc->base.state->adjusted_mode;

	dpp_base = ctx->base + DSS_DPP_OFFSET;
	mctl_sys_base = ctx->base + DSS_MCTRL_SYS_OFFSET;

	outp32(dpp_base + DPP_IMG_SIZE_BEF_SR,
		(DSS_HEIGHT(mode->vdisplay) << 16) | DSS_WIDTH(mode->hdisplay));
	outp32(dpp_base + DPP_IMG_SIZE_AFT_SR,
		(DSS_HEIGHT(mode->vdisplay) << 16) | DSS_WIDTH(mode->hdisplay));

#ifdef CONFIG_HISI_FB_DPP_COLORBAR_USED
	void __iomem *mctl_base;
	outp32(dpp_base + DPP_CLRBAR_CTRL, (0x30 << 24) |(0 << 1) | 0x1);
	set_reg(dpp_base + DPP_CLRBAR_1ST_CLR, 0xFF, 8, 16);
	set_reg(dpp_base + DPP_CLRBAR_2ND_CLR, 0xFF, 8, 8);
	set_reg(dpp_base + DPP_CLRBAR_3RD_CLR, 0xFF, 8, 0);

	mctl_base = ctx->base +
		g_dss_module_ovl_base[DSS_OVL0][MODULE_MCTL_BASE];

	set_reg(mctl_base + MCTL_CTL_MUTEX, 0x1, 1, 0);
	set_reg(mctl_base + MCTL_CTL_EN, 0x1, 32, 0);
	set_reg(mctl_base + MCTL_CTL_TOP, 0x2, 32, 0); /*auto mode*/
	set_reg(mctl_base + MCTL_CTL_DBG, 0xB13A00, 32, 0);

	set_reg(mctl_base + MCTL_CTL_MUTEX_ITF, 0x1, 2, 0);
	set_reg(mctl_sys_base + MCTL_OV0_FLUSH_EN, 0x8, 4, 0);
	set_reg(mctl_base + MCTL_CTL_MUTEX, 0x0, 1, 0);
#endif
}

void enable_ldi(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *ldi_base;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	ldi_base = ctx->base + DSS_LDI0_OFFSET;

	/* ldi enable */
	set_reg(ldi_base + LDI_CTRL, 0x1, 1, 0);
}

void disable_ldi(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *ldi_base;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	ldi_base = ctx->base + DSS_LDI0_OFFSET;

	/* ldi disable */
	set_reg(ldi_base + LDI_CTRL, 0x0, 1, 0);
}

void dpe_interrupt_clear(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *dss_base;
	u32 clear;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	dss_base = ctx->base;

	clear = ~0;
	outp32(dss_base + GLB_CPU_PDP_INTS, clear);
	outp32(dss_base + DSS_LDI0_OFFSET + LDI_CPU_ITF_INTS, clear);
	outp32(dss_base + DSS_DPP_OFFSET + DPP_INTS, clear);

	outp32(dss_base + DSS_DBG_OFFSET + DBG_MCTL_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_WCH0_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_WCH1_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH0_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH1_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH2_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH3_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH4_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH5_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH6_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH7_INTS, clear);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_DSS_GLB_INTS, clear);
}

void dpe_interrupt_unmask(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *dss_base;
	u32 unmask;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	dss_base = ctx->base;

	unmask = ~0;
	unmask &= ~(BIT_DPP_INTS | BIT_ITF0_INTS | BIT_MMU_IRPT_NS);
	outp32(dss_base + GLB_CPU_PDP_INT_MSK, unmask);

	unmask = ~0;
	unmask &= ~(BIT_VSYNC | BIT_VACTIVE0_START
		| BIT_VACTIVE0_END | BIT_FRM_END | BIT_LDI_UNFLOW);

	outp32(dss_base + DSS_LDI0_OFFSET + LDI_CPU_ITF_INT_MSK, unmask);
}

void dpe_interrupt_mask(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *dss_base;
	u32 mask;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return ;
	}

	dss_base = ctx->base;

	mask = ~0;
	outp32(dss_base + GLB_CPU_PDP_INT_MSK, mask);
	outp32(dss_base + DSS_LDI0_OFFSET + LDI_CPU_ITF_INT_MSK, mask);
	outp32(dss_base + DSS_DPP_OFFSET + DPP_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_DSS_GLB_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_MCTL_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_WCH0_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_WCH1_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH0_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH1_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH2_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH3_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH4_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH5_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH6_INT_MSK, mask);
	outp32(dss_base + DSS_DBG_OFFSET + DBG_RCH7_INT_MSK, mask);
}

int dpe_init(struct dss_crtc *acrtc)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;

	mode = &acrtc->base.state->mode;
	adj_mode = &acrtc->base.state->adjusted_mode;

	init_dbuf(acrtc);
	init_dpp(acrtc);
	init_other(acrtc);
	init_ldi(acrtc);

	hisifb_dss_on(acrtc->ctx);
	hisi_dss_mctl_on(acrtc->ctx);

	hisi_dss_mctl_mutex_lock(acrtc->ctx);

	hisi_dss_ovl_base_config(acrtc->ctx, mode->hdisplay, mode->vdisplay);

	hisi_dss_mctl_mutex_unlock(acrtc->ctx);

	enable_ldi(acrtc);

	mdelay(60);

	return 0;
}

void dss_inner_clk_pdp_enable(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *dss_base;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}
	dss_base = ctx->base;

	outp32(dss_base + DSS_IFBC_OFFSET + IFBC_MEM_CTRL, 0x00000088);
	outp32(dss_base + DSS_DSC_OFFSET + DSC_MEM_CTRL, 0x00000888);
	outp32(dss_base + DSS_LDI0_OFFSET + LDI_MEM_CTRL, 0x00000008);
	outp32(dss_base + DSS_DBUF0_OFFSET + DBUF_MEM_CTRL, 0x00000008);
	outp32(dss_base + DSS_DPP_DITHER_OFFSET + DITHER_MEM_CTRL, 0x00000008);
}

void dss_inner_clk_common_enable(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;
	char __iomem *dss_base;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}
	dss_base = ctx->base;

	/*core/axi/mmbuf*/
	outp32(dss_base + DSS_CMDLIST_OFFSET + CMD_MEM_CTRL, 0x00000008);  /*cmd mem*/

	outp32(dss_base + DSS_RCH_VG0_SCL_OFFSET + SCF_COEF_MEM_CTRL, 0x00000088);/*rch_v0 ,scf mem*/
	outp32(dss_base + DSS_RCH_VG0_SCL_OFFSET + SCF_LB_MEM_CTRL, 0x00000008);/*rch_v0 ,scf mem*/
	outp32(dss_base + DSS_RCH_VG0_ARSR_OFFSET + ARSR2P_LB_MEM_CTRL, 0x00000008);/*rch_v0 ,arsr2p mem*/
	outp32(dss_base + DSS_RCH_VG0_DMA_OFFSET + VPP_MEM_CTRL, 0x00000008);/*rch_v0 ,vpp mem*/
	outp32(dss_base + DSS_RCH_VG0_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_v0 ,dma_buf mem*/
	outp32(dss_base + DSS_RCH_VG0_DMA_OFFSET + AFBCD_MEM_CTRL, 0x00008888);/*rch_v0 ,afbcd mem*/

	outp32(dss_base + DSS_RCH_VG1_SCL_OFFSET + SCF_COEF_MEM_CTRL, 0x00000088);/*rch_v1 ,scf mem*/
	outp32(dss_base + DSS_RCH_VG1_SCL_OFFSET + SCF_LB_MEM_CTRL, 0x00000008);/*rch_v1 ,scf mem*/
	outp32(dss_base + DSS_RCH_VG1_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_v1 ,dma_buf mem*/
	outp32(dss_base + DSS_RCH_VG1_DMA_OFFSET + AFBCD_MEM_CTRL, 0x00008888);/*rch_v1 ,afbcd mem*/

	outp32(dss_base + DSS_RCH_VG2_SCL_OFFSET + SCF_COEF_MEM_CTRL, 0x00000088);/*rch_v2 ,scf mem*/
	outp32(dss_base + DSS_RCH_VG2_SCL_OFFSET + SCF_LB_MEM_CTRL, 0x00000008);/*rch_v2 ,scf mem*/
	outp32(dss_base + DSS_RCH_VG2_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_v2 ,dma_buf mem*/

	outp32(dss_base + DSS_RCH_G0_SCL_OFFSET + SCF_COEF_MEM_CTRL, 0x00000088);/*rch_g0 ,scf mem*/
	outp32(dss_base + DSS_RCH_G0_SCL_OFFSET + SCF_LB_MEM_CTRL, 0x0000008);/*rch_g0 ,scf mem*/
	outp32(dss_base + DSS_RCH_G0_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_g0 ,dma_buf mem*/
	outp32(dss_base + DSS_RCH_G0_DMA_OFFSET + AFBCD_MEM_CTRL, 0x00008888);/*rch_g0 ,afbcd mem*/

	outp32(dss_base + DSS_RCH_G1_SCL_OFFSET + SCF_COEF_MEM_CTRL, 0x00000088);/*rch_g1 ,scf mem*/
	outp32(dss_base + DSS_RCH_G1_SCL_OFFSET + SCF_LB_MEM_CTRL, 0x0000008);/*rch_g1 ,scf mem*/
	outp32(dss_base + DSS_RCH_G1_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_g1 ,dma_buf mem*/
	outp32(dss_base + DSS_RCH_G1_DMA_OFFSET + AFBCD_MEM_CTRL, 0x00008888);/*rch_g1 ,afbcd mem*/

	outp32(dss_base + DSS_RCH_D0_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_d0 ,dma_buf mem*/
	outp32(dss_base + DSS_RCH_D0_DMA_OFFSET + AFBCD_MEM_CTRL, 0x00008888);/*rch_d0 ,afbcd mem*/
	outp32(dss_base + DSS_RCH_D1_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_d1 ,dma_buf mem*/
	outp32(dss_base + DSS_RCH_D2_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_d2 ,dma_buf mem*/
	outp32(dss_base + DSS_RCH_D3_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*rch_d3 ,dma_buf mem*/

	outp32(dss_base + DSS_WCH0_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*wch0 DMA/AFBCE mem*/
	outp32(dss_base + DSS_WCH0_DMA_OFFSET + AFBCE_MEM_CTRL, 0x00000888);/*wch0 DMA/AFBCE mem*/
	outp32(dss_base + DSS_WCH0_DMA_OFFSET + ROT_MEM_CTRL, 0x00000008);/*wch0 rot mem*/
	outp32(dss_base + DSS_WCH1_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*wch1 DMA/AFBCE mem*/
	outp32(dss_base + DSS_WCH1_DMA_OFFSET + AFBCE_MEM_CTRL, 0x00000888);/*wch1 DMA/AFBCE mem*/
	outp32(dss_base + DSS_WCH1_DMA_OFFSET + ROT_MEM_CTRL, 0x00000008);/*wch1 rot mem*/
	outp32(dss_base + DSS_WCH2_DMA_OFFSET + DMA_BUF_MEM_CTRL, 0x00000008);/*wch2 DMA/AFBCE mem*/
	outp32(dss_base + DSS_WCH2_DMA_OFFSET + ROT_MEM_CTRL, 0x00000008);/*wch2 rot mem*/
}
int dpe_irq_enable(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	if (ctx->irq)
		enable_irq(ctx->irq);

	return 0;
}

int dpe_irq_disable(struct dss_crtc *acrtc)
{
	struct dss_hw_ctx *ctx;

	ctx = acrtc->ctx;
	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	if (ctx->irq)
		disable_irq(ctx->irq);

	/*disable_irq_nosync(ctx->irq);*/

	return 0;
}

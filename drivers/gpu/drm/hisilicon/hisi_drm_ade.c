/*
 * Hisilicon Hi6220 SoC ADE(Advanced Display Engine)'s crtc&plane driver
 *
 * Copyright (c) 2014-2015 Hisilicon Limited.
 * Author:
 *	Xinliang Liu <xinliang.liu@linaro.org>
 *	Xinliang Liu <z.liuxinliang@hisilicon.com>
 *	Xinwei Kong <kong.kongxinwei@hisilicon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <video/display_timing.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

#include "hisi_drm_drv.h"
#include "hisi_ade_reg.h"

#define FORCE_PIXEL_CLOCK_SAME_OR_HIGHER 0
#define PRIMARY_CH	(ADE_CH1)

#define to_ade_crtc(crtc) \
	container_of(crtc, struct ade_crtc, base)

#define to_ade_plane(plane) \
	container_of(plane, struct ade_plane, base)

struct ade_hw_ctx {
	void __iomem  *base;
	void __iomem  *media_base;
	void __iomem  *media_noc_base;

	int irq;
	u32 ade_core_rate;
	u32 media_noc_rate;

	struct clk *ade_core_clk;
	struct clk *media_noc_clk;
	struct clk *ade_pix_clk;
	bool power_on;
};

struct ade_crtc {
	struct drm_crtc base;
	struct ade_hw_ctx *ctx;
	bool enable;
	u64 use_mask;
};

struct ade_plane {
	struct drm_plane base;
	void *ctx;
	u8 ch; /* channel */
};

struct ade_data {
	struct ade_crtc acrtc;
	struct ade_plane aplane[ADE_CH_NUM];
	struct ade_hw_ctx ctx;
};

/* ade-format info: */
struct ade_format {
	u32 pixel_format;
	enum ADE_FORMAT ade_format;
};

static const struct ade_format ade_formats[] = {
	/* 16bpp RGB: */
	{ DRM_FORMAT_RGB565, ADE_RGB_565 },
	{ DRM_FORMAT_BGR565, ADE_BGR_565 },
	/* 24bpp RGB: */
	{ DRM_FORMAT_RGB888, ADE_RGB_888 },
	{ DRM_FORMAT_BGR888, ADE_BGR_888 },
	/* 32bpp [A]RGB: */
	{ DRM_FORMAT_XRGB8888, ADE_XRGB_8888 },
	{ DRM_FORMAT_XBGR8888, ADE_XBGR_8888 },
	{ DRM_FORMAT_RGBA8888, ADE_RGBA_8888 },
	{ DRM_FORMAT_BGRA8888, ADE_BGRA_8888 },
	{ DRM_FORMAT_ARGB8888, ADE_ARGB_8888 },
	{ DRM_FORMAT_ABGR8888, ADE_ABGR_8888 },
};

static const u32 channel_formats1[] = {
	/* channel 1,2,3,4 */
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565, DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888, DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888, DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888
};

u32 ade_get_channel_formats(u8 ch, const u32 **formats)
{
	switch (ch) {
	case ADE_CH1:
		*formats = channel_formats1;
		return ARRAY_SIZE(channel_formats1);
	default:
		DRM_ERROR("no this channel %d\n", ch);
		*formats = NULL;
		return 0;
	}
}

/* convert from fourcc format to ade format */
static u32 ade_get_format(u32 pixel_format)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ade_formats); i++)
		if (ade_formats[i].pixel_format == pixel_format)
			return ade_formats[i].ade_format;

	/* not found */
	DRM_ERROR("Not found pixel format!!fourcc_format= %d\n", pixel_format);
	return ADE_FORMAT_NOT_SUPPORT;
}

static void ade_init(struct ade_hw_ctx *ctx)
{
	void __iomem *base = ctx->base;

	/* enable clk gate */
	set_TOP_CTL_clk_gate_en(base, 1);
	/* clear overlay */
	writel(0, base + ADE_OVLY1_TRANS_CFG);
	writel(0, base + ADE_OVLY_CTL);
	writel(0, base + ADE_OVLYX_CTL(ADE_OVLY2));
	/* clear reset and reload regs */
	writel(0, base + ADE_SOFT_RST_SEL0);
	writel(0, base + ADE_SOFT_RST_SEL1);
	writel(0xFFFFFFFF, base + ADE_RELOAD_DIS0);
	writel(0xFFFFFFFF, base + ADE_RELOAD_DIS1);
	/* for video set to 1, means that ade registers
	 * became effective at frame end
	 */
	set_TOP_CTL_frm_end_start(base, 1);
}

static void ade_ldi_set_mode(struct ade_crtc *acrtc,
			     struct drm_display_mode *mode,
			     struct drm_display_mode *adj_mode)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 out_w = mode->hdisplay;
	u32 out_h = mode->vdisplay;
	u32 hfp, hbp, hsw, vfp, vbp, vsw;
	u32 plr_flags;
	int ret;

	plr_flags = (mode->flags & DRM_MODE_FLAG_NVSYNC)
			? HISI_LDI_FLAG_NVSYNC : 0;
	plr_flags |= (mode->flags & DRM_MODE_FLAG_NHSYNC)
			? HISI_LDI_FLAG_NHSYNC : 0;
	hfp = mode->hsync_start - mode->hdisplay;
	hbp = mode->htotal - mode->hsync_end;
	hsw = mode->hsync_end - mode->hsync_start;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;
	vsw = mode->vsync_end - mode->vsync_start;
	if (vsw > 15) {
		DRM_INFO("vsw exceeded 15\n");
		vsw = 15;
	}

	writel((hbp << 20) | (hfp << 0), base + LDI_HRZ_CTRL0);
	/* p3-73 6220V100 pdf:
	 *  "The configured value is the actual width - 1"
	 */
	writel(hsw - 1, base + LDI_HRZ_CTRL1);
	writel((vbp << 20) | (vfp << 0), base + LDI_VRT_CTRL0);
	/* p3-74 6220V100 pdf:
	 *  "The configured value is the actual width - 1"
	 */
	writel(vsw - 1, base + LDI_VRT_CTRL1);

	/* p3-75 6220V100 pdf:
	 *  "The configured value is the actual width - 1"
	 */
	writel(((out_h - 1) << 20) | ((out_w - 1) << 0),
	       base + LDI_DSP_SIZE);
	writel(plr_flags, base + LDI_PLR_CTRL);

	ret = clk_set_rate(ctx->ade_pix_clk, mode->clock * 1000);
	/* Success should be guaranteed in aotomic_check
	 * failer shouldn't happen here
	 */
	if (ret)
		DRM_ERROR("set ade_pixel_clk_rate fail\n");
	adj_mode->clock = clk_get_rate(ctx->ade_pix_clk) / 1000;

	/* ctran6 setting */
	writel(1, base + ADE_CTRAN_DIS(ADE_CTRAN6));
	writel(out_w * out_h - 1, base + ADE_CTRAN_IMAGE_SIZE(ADE_CTRAN6));
	acrtc->use_mask |= BIT(ADE_CTRAN_BIT_OFST + ADE_CTRAN6);
	DRM_INFO("set mode: %dx%d\n", out_w, out_h);

	/*
	 * other parameters setting
	 */
	writel(BIT(0), base + LDI_WORK_MODE);
	writel((0x3c << 6) | (ADE_OUT_RGB_888 << 3) | BIT(2) | BIT(0),
	       base + LDI_CTRL);
	set_reg(base + LDI_DE_SPACE_LOW, 0x1, 1, 1);
}

static int ade_power_up(struct ade_hw_ctx *ctx)
{
	void __iomem *media_base = ctx->media_base;
	int ret;

	ret = clk_set_rate(ctx->ade_core_clk, ctx->ade_core_rate);
	if (ret) {
		DRM_ERROR("clk_set_rate ade_core_rate error\n");
		return ret;
	}
	ret = clk_set_rate(ctx->media_noc_clk, ctx->media_noc_rate);
	if (ret) {
		DRM_ERROR("media_noc_clk media_noc_rate error\n");
		return ret;
	}
	ret = clk_prepare_enable(ctx->media_noc_clk);
	if (ret) {
		DRM_ERROR("fail to clk_prepare_enable media_noc_clk\n");
		return ret;
	}

	writel(0x20, media_base + SC_MEDIA_RSTDIS);

	ret = clk_prepare_enable(ctx->ade_core_clk);
	if (ret) {
		DRM_ERROR("fail to clk_prepare_enable ade_core_clk\n");
		return ret;
	}

	ade_init(ctx);
	ctx->power_on = true;
	return 0;
}

static void ade_power_down(struct ade_hw_ctx *ctx)
{
	void __iomem *base = ctx->base;
	void __iomem *media_base = ctx->media_base;

	set_LDI_CTRL_ldi_en(base, ADE_DISABLE);
	/* dsi pixel off */
	set_reg(base + LDI_HDMI_DSI_GT, 0x1, 1, 0);

	clk_disable_unprepare(ctx->ade_core_clk);
	writel(0x20, media_base + SC_MEDIA_RSTEN);
	clk_disable_unprepare(ctx->media_noc_clk);
	ctx->power_on = false;
}

static struct drm_crtc *hisi_get_crtc_from_index(struct drm_device *dev,
						 unsigned int index)
{
	unsigned int index_tmp = 0;
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (index_tmp == index)
			return crtc;

		index_tmp++;
	}

	WARN_ON(true);
	return NULL;
}

int ade_enable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct drm_crtc *crtc = hisi_get_crtc_from_index(dev, pipe);
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 intr_en;

	DRM_INFO("enable_vblank enter.\n");
	if (!ctx->power_on)
		(void)ade_power_up(ctx);

	intr_en = readl(base + LDI_INT_EN);
	intr_en |= LDI_ISR_FRAME_END_INT;
	writel(intr_en, base + LDI_INT_EN);

	return 0;
}

void ade_disable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct drm_crtc *crtc = hisi_get_crtc_from_index(dev, pipe);
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 intr_en;

	DRM_INFO("disable_vblank enter.\n");
	if (!ctx->power_on) {
		DRM_ERROR("power is down! vblank disable fail\n");
		return;
	}
	intr_en = readl(base + LDI_INT_EN);
	intr_en &= ~LDI_ISR_FRAME_END_INT;
	writel(intr_en, base + LDI_INT_EN);
}

static irqreturn_t ade_irq_handler(int irq, void *data)
{
	struct ade_crtc *acrtc = data;
	struct ade_hw_ctx *ctx = acrtc->ctx;
	struct drm_crtc *crtc = &acrtc->base;
	struct drm_device *dev = crtc->dev;
	void __iomem *base = ctx->base;
	u32 status;

	status = readl(base + LDI_MSK_INT);
	/* DRM_INFO("LDI IRQ: status=0x%X\n",status); */

	/* vblank irq */
	if (status & LDI_ISR_FRAME_END_INT) {
		writel(LDI_ISR_FRAME_END_INT, base + LDI_INT_CLR);
		drm_handle_vblank(dev, drm_crtc_index(crtc));
	}

	return IRQ_HANDLED;
}

/*
 * set modules' reset mode: by software or hardware
 * set modules' reload enable/disable
 */
static void ade_set_reset_and_reload(struct ade_crtc *acrtc)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 mask0 = (u32)acrtc->use_mask;
	u32 mask1 = (u32)(acrtc->use_mask >> 32);

	DRM_DEBUG_DRIVER("mask=0x%llX, mask0=0x%X, mask1=0x%X\n",
			 acrtc->use_mask, mask0, mask1);

	writel(mask0, base + ADE_SOFT_RST_SEL0);
	writel(mask1, base + ADE_SOFT_RST_SEL1);
	writel(~mask0, base + ADE_RELOAD_DIS0);
	writel(~mask1, base + ADE_RELOAD_DIS1);
}

void ade_set_medianoc_qos(struct ade_crtc *acrtc)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->media_noc_base;
	void __iomem *reg;
	u32 val;

	reg = base + NOC_ADE0_QOSGENERATOR_MODE;
	val = (readl(reg) & 0xfffffffc) | 0x2;
	writel(val, reg);

	reg = base + NOC_ADE0_QOSGENERATOR_EXTCONTROL;
	val = readl(reg) | 0x1;
	writel(val, reg);

	reg = base + NOC_ADE1_QOSGENERATOR_MODE;
	val = (readl(reg) & 0xfffffffc) | 0x2;
	writel(val, reg);

	reg = base + NOC_ADE1_QOSGENERATOR_EXTCONTROL;
	val = readl(reg) | 0x1;
	writel(val, reg);
}

/*
 * commit to ldi to display
 */
static void ade_display_commit(struct ade_crtc *acrtc)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;

	/* TODO: set rotator after overlay */

	/* TODO: set scale after overlay */

	/* display source setting */
	writel(TOP_DISP_SRC_OVLY2, base + ADE_DISP_SRC_CFG);

	/* set reset mode:soft or hw, and reload modules */
	ade_set_reset_and_reload(acrtc);

	DRM_INFO("ADE GO\n");
	/* enable ade */
	wmb();
	writel(ADE_ENABLE, base + ADE_EN);
	/* enable ldi */
	wmb();
	set_LDI_CTRL_ldi_en(base, ADE_ENABLE);
	/* dsi pixel on */
	set_reg(base + LDI_HDMI_DSI_GT, 0x0, 1, 0);
}

static void ade_crtc_enable(struct drm_crtc *crtc)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	int ret;

	DRM_DEBUG_DRIVER("enter.\n");
	if (acrtc->enable)
		return;

	if (!ctx->power_on) {
		ret = ade_power_up(ctx);
		if (ret) {
			DRM_ERROR("failed to initialize ade clk\n");
			return;
		}
	}

	ade_set_medianoc_qos(acrtc);
	ade_display_commit(acrtc);
	acrtc->enable = true;

	DRM_DEBUG_DRIVER("exit success.\n");
}

static void ade_crtc_disable(struct drm_crtc *crtc)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;

	DRM_DEBUG_DRIVER("enter.\n");

	if (!acrtc->enable)
		return;

	ade_power_down(ctx);
	acrtc->use_mask = 0;
	acrtc->enable = false;
	DRM_DEBUG_DRIVER("exit success.\n");
}

int ade_crtc_atomic_check(struct drm_crtc *crtc, struct drm_crtc_state *state)
{
	DRM_DEBUG_DRIVER("enter.\n");
	DRM_DEBUG_DRIVER("exit success.\n");
	/* do nothing */
	return 0;
}

static void ade_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	struct drm_display_mode *mode = &crtc->state->mode;
	struct drm_display_mode *adj_mode = &crtc->state->adjusted_mode;

	DRM_DEBUG_DRIVER("enter.\n");
	if (!ctx->power_on)
		(void)ade_power_up(ctx);
	ade_ldi_set_mode(acrtc, mode, adj_mode);
	DRM_DEBUG_DRIVER("exit success.\n");
}

static void ade_crtc_atomic_begin(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)
{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;

	DRM_DEBUG_DRIVER("enter.\n");
	if (!ctx->power_on)
		(void)ade_power_up(ctx);
	DRM_DEBUG_DRIVER("exit success.\n");
}

static void ade_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_crtc_state *old_state)

{
	struct ade_crtc *acrtc = to_ade_crtc(crtc);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;

	DRM_DEBUG_DRIVER("enter.\n");
	/* commit to  display: LDI input setting */
	if (acrtc->enable) {
		/* set reset and reload */
		ade_set_reset_and_reload(acrtc);
		/* flush ade regitsters */
		wmb();
		writel(ADE_ENABLE, base + ADE_EN);
	}
	DRM_DEBUG_DRIVER("exit success.\n");
}

static const struct drm_crtc_helper_funcs ade_crtc_helper_funcs = {
	.enable		= ade_crtc_enable,
	.disable	= ade_crtc_disable,
	.atomic_check	= ade_crtc_atomic_check,
	.mode_set_nofb	= ade_crtc_mode_set_nofb,
	.atomic_begin	= ade_crtc_atomic_begin,
	.atomic_flush	= ade_crtc_atomic_flush,
};

static const struct drm_crtc_funcs ade_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= drm_atomic_helper_crtc_reset,
	.set_property = drm_atomic_helper_crtc_set_property,
	.atomic_duplicate_state	= drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
};

static int ade_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
			 struct drm_plane *plane)
{
	int ret;

	ret = drm_crtc_init_with_planes(dev, crtc, plane,
					NULL, &ade_crtc_funcs);
	if (ret) {
		DRM_ERROR("failed to init crtc.\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &ade_crtc_helper_funcs);

	return 0;
}

static void ade_rdma_set(struct ade_crtc *acrtc, struct drm_framebuffer *fb,
			 u32 ch, u32 y, u32 in_h, u32 fmt)
{
	u32 reg_ctrl, reg_addr, reg_size, reg_stride, reg_space, reg_en;
	struct drm_gem_cma_object *obj = drm_fb_cma_get_gem_obj(fb, 0);
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 stride = fb->pitches[0];
	u32 addr = (u32)obj->paddr + y * stride;

	DRM_DEBUG_DRIVER("rdma%d: (y=%d, height=%d), stride=%d, paddr=0x%x, \
			 addr=0x%x, fb:%dx%d, pixel_format=%d(%s)\n",
			 ch + 1, y, in_h, stride, (u32)obj->paddr,
			 addr, fb->width, fb->height,
			 fmt, drm_get_format_name(fb->pixel_format));

	/* get reg offset */
	reg_ctrl = RD_CH_CTRL(ch);
	reg_addr = RD_CH_ADDR(ch);
	reg_size = RD_CH_SIZE(ch);
	reg_stride = RD_CH_STRIDE(ch);
	reg_space = RD_CH_SPACE(ch);
	reg_en = RD_CH_EN(ch);

	/*
	 * TODO: set rotation
	 */
	writel((fmt << 16) & 0x1f0000, base + reg_ctrl);
	writel(addr, base + reg_addr);
	writel((in_h << 16) | stride, base + reg_size);
	writel(stride, base + reg_stride);
	writel(in_h * stride, base + reg_space);
	writel(1, base + reg_en);

	acrtc->use_mask |= BIT(ADE_CH_RDMA_BIT_OFST + ch);
}

static void ade_rdma_disable(struct ade_crtc *acrtc, u32 ch)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 reg_en;

	/* get reg offset */
	reg_en = RD_CH_EN(ch);

	writel(0, base + reg_en);
	acrtc->use_mask &= ~BIT(ADE_CH_RDMA_BIT_OFST + ch);
}

static void ade_clip_set(struct ade_crtc *acrtc, u32 ch, u32 fb_w, u32 x,
			 u32 in_w, u32 in_h)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u32 disable_val;
	u32 clip_left;
	u32 clip_right;

	/*
	 * clip width, no need to clip height
	 */
	if (fb_w == in_w) { /* bypass */
		disable_val = 1;
		clip_left = 0;
		clip_right = 0;
	} else {
		disable_val = 0;
		clip_left = x;
		clip_right = fb_w - (x + in_w) - 1;
	}

	DRM_DEBUG_DRIVER("clip%d: clip_left=%d, clip_right=%d\n",
			 ch + 1, clip_left, clip_right);

	writel(disable_val, base + ADE_CLIP_DISABLE(ch));
	writel((fb_w - 1) << 16 | (in_h - 1), base + ADE_CLIP_SIZE0(ch));
	writel(clip_left << 16 | clip_right, base + ADE_CLIP_SIZE1(ch));

	acrtc->use_mask |= BIT(ADE_CLIP_BIT_OFST + ch);
}

static void ade_clip_disable(struct ade_crtc *acrtc, u32 ch)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;

	writel(1, base + ADE_CLIP_DISABLE(ch));
	acrtc->use_mask &= ~BIT(ADE_CLIP_BIT_OFST + ch);
}

static bool has_Alpha_channel(int format)
{
	switch (format) {
	case ADE_ARGB_8888:
	case ADE_ABGR_8888:
	case ADE_RGBA_8888:
	case ADE_BGRA_8888:
		return true;
	default:
		return false;
	}
}

static void ade_get_blending_params(u32 fmt, u8 glb_alpha, u8 *alp_mode,
				    u8 *alp_sel, u8 *under_alp_sel)
{
	bool has_alpha = has_Alpha_channel(fmt);

	/*
	 * get alp_mode
	 */
	if (has_alpha && glb_alpha < 255)
		*alp_mode = ADE_ALP_PIXEL_AND_GLB;
	else if (has_alpha)
		*alp_mode = ADE_ALP_PIXEL;
	else
		*alp_mode = ADE_ALP_GLOBAL;

	/*
	 * get alp sel
	 */
	*alp_sel = ADE_ALP_MUL_COEFF_3; /* 1 */
	*under_alp_sel = ADE_ALP_MUL_COEFF_2; /* 0 */
}

static void ade_overlay_set(struct ade_crtc *acrtc, u8 ch, u32 x0, u32 y0,
			    u32 in_w, u32 in_h, u32 fmt)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u8 ovly_ch = 0;
	u8 x = ADE_OVLY2;
	u8 glb_alpha = 255;
	u32 x1 = x0 + in_w - 1;
	u32 y1 = y0 + in_h - 1;
	u32 val;
	u8 alp_sel;
	u8 under_alp_sel;
	u8 alp_mode;

	ade_get_blending_params(fmt, glb_alpha, &alp_mode, &alp_sel,
				&under_alp_sel);

	/* overlay routing setting */
	writel(x0 << 16 | y0, base + ADE_OVLY_CH_XY0(ovly_ch));
	writel(x1 << 16 | y1, base + ADE_OVLY_CH_XY1(ovly_ch));
	val = (ch + 1) << ADE_OVLY_CH_SEL_OFST | BIT(ADE_OVLY_CH_EN_OFST) |
		alp_sel << ADE_OVLY_CH_ALP_SEL_OFST |
		under_alp_sel << ADE_OVLY_CH_UNDER_ALP_SEL_OFST |
		glb_alpha << ADE_OVLY_CH_ALP_GBL_OFST |
		alp_mode << ADE_OVLY_CH_ALP_MODE_OFST;
	DRM_DEBUG_DRIVER("ch%d_ctl=0x%X\n", ovly_ch + 1, val);
	writel(val, base + ADE_OVLY_CH_CTL(ovly_ch));
	val = (x + 1) << (ovly_ch * 4) | readl(base + ADE_OVLY_CTL);
	DRM_DEBUG_DRIVER("ovly_ctl=0x%X\n", val);
	writel(val, base + ADE_OVLY_CTL);

	/* when primary is enable, indicate that it's ready to output. */
	if (ch == PRIMARY_CH) {
		val = (in_w - 1) << 16 | (in_h - 1);
		writel(val, base + ADE_OVLY_OUTPUT_SIZE(x));
		writel(1, base + ADE_OVLYX_CTL(x));
		acrtc->use_mask |= BIT(ADE_OVLY_BIT_OFST + x);
	}
}

static void ade_overlay_disable(struct ade_crtc *acrtc, u32 ch)
{
	struct ade_hw_ctx *ctx = acrtc->ctx;
	void __iomem *base = ctx->base;
	u8 ovly_ch = 0;
	u32 val;

	val = ~BIT(6) & readl(base + ADE_OVLY_CH_CTL(ovly_ch));
	DRM_DEBUG_DRIVER("ch%d_ctl=0x%X\n", ovly_ch + 1, val);
	writel(val, base + ADE_OVLY_CH_CTL(ovly_ch));
	val = ~(0x3 << (ovly_ch * 4)) & readl(base + ADE_OVLY_CTL);

	DRM_DEBUG_DRIVER("ovly_ctl=0x%X\n", val);
	writel(val, base + ADE_OVLY_CTL);
}

/*
 * Typicaly, a channel looks like: DMA-->clip-->scale-->ctrans-->overlay
 */
static void ade_update_channel(struct ade_plane *aplane, struct ade_crtc *acrtc,
			       struct drm_framebuffer *fb, int crtc_x,
			       int crtc_y, unsigned int crtc_w,
			       unsigned int crtc_h, u32 src_x,
			       u32 src_y, u32 src_w, u32 src_h)
{
	u8 ch = aplane->ch;
	u32 fmt = ade_get_format(fb->pixel_format);
	u32 in_w;
	u32 in_h;

	DRM_DEBUG_DRIVER("channel%d: src:(%d, %d)-%dx%d, crtc:(%d, %d)-%dx%d",
			 ch + 1, src_x, src_y, src_w, src_h,
			 crtc_x, crtc_y, crtc_w, crtc_h);

	/* 1) DMA setting */
	in_w = src_w;
	in_h = src_h;
	ade_rdma_set(acrtc, fb, ch, src_y, in_h, fmt);

	/* 2) clip setting */
	ade_clip_set(acrtc, ch, fb->width, src_x, in_w, in_h);

	/* 3) TODO: scale setting for overlay planes */

	/* 4) TODO: ctran/csc setting for overlay planes */

	/* 5) overlay/compositor routing setting */
	ade_overlay_set(acrtc, ch, crtc_x, crtc_y, in_w, in_h, fmt);

	DRM_DEBUG_DRIVER("exit success.\n");
}

static void ade_disable_channel(struct ade_plane *aplane,
				struct ade_crtc *acrtc)
{
	u32 ch = aplane->ch;

	DRM_DEBUG_DRIVER("disable channel%d\n", ch + 1);

	/*
	 * when primary is disable, power is down
	 * so no need to disable this channel.
	 */
	if (ch == PRIMARY_CH)
		return;

	/* disable read DMA */
	ade_rdma_disable(acrtc, ch);

	/* disable clip */
	ade_clip_disable(acrtc, ch);

	/* disable overlay routing */
	ade_overlay_disable(acrtc, ch);

	DRM_DEBUG_DRIVER("exit success.\n");
}

static int ade_plane_prepare_fb(struct drm_plane *plane,
				const struct drm_plane_state *new_state)
{
	DRM_DEBUG_DRIVER("enter.\n");
	DRM_DEBUG_DRIVER("exit success.\n");
	return 0;
}

static void ade_plane_cleanup_fb(struct drm_plane *plane,
				 const struct drm_plane_state *old_state)
{
	DRM_DEBUG_DRIVER("enter.\n");
	DRM_DEBUG_DRIVER("exit success.\n");
}

static int ade_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	struct drm_framebuffer *fb = state->fb;
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	u32 src_x = state->src_x >> 16;
	u32 src_y = state->src_y >> 16;
	u32 src_w = state->src_w >> 16;
	u32 src_h = state->src_h >> 16;
	int crtc_x = state->crtc_x;
	int crtc_y = state->crtc_y;
	u32 crtc_w = state->crtc_w;
	u32 crtc_h = state->crtc_h;

	if (!crtc || !fb)
		return 0;

	crtc_state = drm_atomic_get_crtc_state(state->state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	if (src_w != crtc_w || src_h != crtc_h) {
		DRM_ERROR("Scale not support!!!\n");
		return -EINVAL;
	}

	if (src_x + src_w > fb->width ||
	    src_y + src_h > fb->height)
		return -EINVAL;

	if (crtc_x < 0 || crtc_y < 0)
		return -EINVAL;

	if (crtc_x + crtc_w > crtc_state->adjusted_mode.hdisplay ||
	    crtc_y + crtc_h > crtc_state->adjusted_mode.vdisplay)
		return -EINVAL;

	return 0;
}

static void ade_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct drm_plane_state	*state	= plane->state;
	struct ade_plane *aplane = to_ade_plane(plane);
	struct ade_crtc *acrtc;

	if (!state->crtc)
		return;

	acrtc = to_ade_crtc(state->crtc);
	ade_update_channel(aplane, acrtc, state->fb,
			   state->crtc_x, state->crtc_y,
			   state->crtc_w, state->crtc_h,
			   state->src_x >> 16, state->src_y >> 16,
			   state->src_w >> 16, state->src_h >> 16);
}

static void ade_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct ade_plane *aplane = to_ade_plane(plane);
	struct ade_crtc *acrtc;

	if (!old_state->crtc)
		return;
	acrtc = to_ade_crtc(old_state->crtc);
	ade_disable_channel(aplane, acrtc);
}

static const struct drm_plane_helper_funcs ade_plane_helper_funcs = {
	.prepare_fb = ade_plane_prepare_fb,
	.cleanup_fb = ade_plane_cleanup_fb,
	.atomic_check = ade_plane_atomic_check,
	.atomic_update = ade_plane_atomic_update,
	.atomic_disable = ade_plane_atomic_disable,
};

static struct drm_plane_funcs ade_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.set_property = drm_atomic_helper_plane_set_property,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static int ade_plane_init(struct drm_device *dev, struct ade_plane *aplane,
			  enum drm_plane_type type)
{
	const u32 *fmts;
	u32 fmts_cnt;
	int ret = 0;

	/* get  properties */
	fmts_cnt = ade_get_channel_formats(aplane->ch, &fmts);
	if (ret)
		return ret;

	ret = drm_universal_plane_init(dev, &aplane->base, 1, &ade_plane_funcs,
				       fmts, fmts_cnt, type);
	if (ret) {
		DRM_ERROR("fail to init plane, ch=%d\n", aplane->ch);
		return ret;
	}

	drm_plane_helper_add(&aplane->base, &ade_plane_helper_funcs);

	return 0;
}

static int ade_bind(struct device *dev, struct device *master, void *data)
{
	struct ade_data *ade = dev_get_drvdata(dev);
	struct ade_hw_ctx *ctx = &ade->ctx;
	struct ade_crtc *acrtc = &ade->acrtc;
	struct drm_device *drm_dev = (struct drm_device *)data;
	struct ade_plane *aplane;
	enum drm_plane_type type;
	int ret;
	int i;

	/*
	 * plane init
	 * TODO: Now only support primary plane, overlay planes
	 * need to do.
	 */
	for (i = 0; i < ADE_CH_NUM; i++) {
		aplane = &ade->aplane[i];
		aplane->ch = i;
		aplane->ctx = ctx;
		type = i == PRIMARY_CH ? DRM_PLANE_TYPE_PRIMARY :
			DRM_PLANE_TYPE_OVERLAY;

		ret = ade_plane_init(drm_dev, aplane, type);
		if (ret)
			return ret;
	}

	/* crtc init */
	acrtc->ctx = ctx;
	ret = ade_crtc_init(drm_dev, &acrtc->base,
			    &ade->aplane[PRIMARY_CH].base);
	if (ret)
		return ret;

	/* vblank irq init */
	ret = request_irq(ctx->irq, ade_irq_handler, DRIVER_IRQ_SHARED,
			  drm_dev->driver->name, acrtc);
	if (ret)
		return ret;

	return 0;
}

static void ade_unbind(struct device *dev, struct device *master, void *data)
{
	/* do nothing */
}

static const struct component_ops ade_ops = {
	.bind	= ade_bind,
	.unbind	= ade_unbind,
};

static int ade_dts_parse(struct platform_device *pdev, struct ade_hw_ctx *ctx)
{
	struct resource *res;
	struct device *dev;
	struct device_node *np;
	int ret;

	dev = &pdev->dev;
	np  = dev->of_node;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ade_base");
	ctx->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ctx->base)) {
		DRM_ERROR("failed to remap ade io base\n");
		return  PTR_ERR(ctx->base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "media_base");
	ctx->media_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ctx->media_base)) {
		DRM_ERROR("failed to remap media io base\n");
		return PTR_ERR(ctx->media_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "media_noc_base");
	ctx->media_noc_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ctx->media_noc_base)) {
		DRM_ERROR("failed to remap media noc base\n");
		return PTR_ERR(ctx->media_noc_base);
	}

	ctx->irq = platform_get_irq(pdev, 0);
	if (ctx->irq < 0) {
		DRM_ERROR("failed to parse the irq\n");
		return -ENODEV;
	}

	ctx->ade_core_clk = devm_clk_get(&pdev->dev, "clk_ade_core");
	if (!ctx->ade_core_clk) {
		DRM_ERROR("failed to parse the ADE_CORE\n");
		return -ENODEV;
	}
	ctx->media_noc_clk = devm_clk_get(&pdev->dev,
					"aclk_codec_jpeg_src");
	if (!ctx->media_noc_clk) {
		DRM_ERROR("failed to parse the CODEC_JPEG\n");
	    return -ENODEV;
	}
	ctx->ade_pix_clk = devm_clk_get(&pdev->dev, "clk_ade_pix");
	if (!ctx->ade_pix_clk) {
		DRM_ERROR("failed to parse the ADE_PIX_SRC\n");
	    return -ENODEV;
	}

	ret = of_property_read_u32(np, "ade_core_clk_rate",
				   &ctx->ade_core_rate);
	if (ret) {
		DRM_ERROR("failed to parse the ade_core_clk_rate\n");
	    return -ENODEV;
	}
	ret = of_property_read_u32(np, "media_noc_clk_rate",
				   &ctx->media_noc_rate);
	if (ret) {
		DRM_ERROR("failed to parse the media_noc_clk_rate\n");
		return -ENODEV;
	}

	return 0;
}

static int ade_probe(struct platform_device *pdev)
{
	struct ade_data *ade;
	int ret;

	DRM_DEBUG_DRIVER("enter.\n");

	ade = devm_kzalloc(&pdev->dev, sizeof(*ade), GFP_KERNEL);
	if (!ade) {
		DRM_ERROR("failed to alloc ade_data\n");
		return -ENOMEM;
	}

	ret = ade_dts_parse(pdev, &ade->ctx);
	if (ret) {
		DRM_ERROR("failed to parse dts!!\n");
		return ret;
	}

	platform_set_drvdata(pdev, ade);

	return component_add(&pdev->dev, &ade_ops);
}

static int ade_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &ade_ops);

	return 0;
}

static const struct of_device_id ade_of_match[] = {
	{ .compatible = "hisilicon,hi6220-ade" },
	{ }
};
MODULE_DEVICE_TABLE(of, ade_of_match);

static struct platform_driver ade_driver = {
	.probe = ade_probe,
	.remove = ade_remove,
	.driver = {
		   .name = "hisi-ade",
		   .owner = THIS_MODULE,
		   .of_match_table = ade_of_match,
	},
};

module_platform_driver(ade_driver);

MODULE_AUTHOR("Xinliang Liu <xinliang.liu@linaro.org>");
MODULE_AUTHOR("Xinliang Liu <z.liuxinliang@hisilicon.com>");
MODULE_AUTHOR("Xinwei Kong <kong.kongxinwei@hisilicon.com>");
MODULE_DESCRIPTION("Hisilicon DRM ADE(crtc/plane) Driver");
MODULE_LICENSE("GPL v2");

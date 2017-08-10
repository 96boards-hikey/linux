/* Copyright (c) 2008-2011, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

#include "kirin_drm_dpe_utils.h"
#include "kirin_drm_drv.h"


#define DSS_CHN_MAX_DEFINE (DSS_COPYBIT_MAX)
static int mid_array[DSS_CHN_MAX_DEFINE] = {0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x2, 0x1, 0x3, 0x0};

/*
** dss_chn_idx
** DSS_RCHN_D2 = 0,	DSS_RCHN_D3,	DSS_RCHN_V0,	DSS_RCHN_G0,	DSS_RCHN_V1,
** DSS_RCHN_G1,	DSS_RCHN_D0,	DSS_RCHN_D1,	DSS_WCHN_W0,	DSS_WCHN_W1,
** DSS_RCHN_V2,   DSS_WCHN_W2,
*/
/*lint -e785*/
u32 g_dss_module_base[DSS_CHN_MAX_DEFINE][MODULE_CHN_MAX] = {
	/* D0 */
	{
	MIF_CH0_OFFSET,
	AIF0_CH0_OFFSET,
	AIF1_CH0_OFFSET,
	MCTL_CTL_MUTEX_RCH0,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH0_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH0_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH0_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD0_DBG,
	DSS_RCH_D0_DMA_OFFSET,
	DSS_RCH_D0_DFC_OFFSET,
	0,
	0,
	0,
	0,
	0,
	0,
	DSS_RCH_D0_CSC_OFFSET,
	},

	/* D1 */
	{
	MIF_CH1_OFFSET,
	AIF0_CH1_OFFSET,
	AIF1_CH1_OFFSET,
	MCTL_CTL_MUTEX_RCH1,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH1_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH1_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH1_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD1_DBG,
	DSS_RCH_D1_DMA_OFFSET,
	DSS_RCH_D1_DFC_OFFSET,
	0,
	0,
	0,
	0,
	0,
	0,
	DSS_RCH_D1_CSC_OFFSET,
	},

	/* V0 */
	{
	MIF_CH2_OFFSET,
	AIF0_CH2_OFFSET,
	AIF1_CH2_OFFSET,
	MCTL_CTL_MUTEX_RCH2,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH2_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH2_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH2_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD2_DBG,
	DSS_RCH_VG0_DMA_OFFSET,
	DSS_RCH_VG0_DFC_OFFSET,
	DSS_RCH_VG0_SCL_OFFSET,
	DSS_RCH_VG0_SCL_LUT_OFFSET,
	DSS_RCH_VG0_ARSR_OFFSET,
	DSS_RCH_VG0_ARSR_LUT_OFFSET,
	DSS_RCH_VG0_POST_CLIP_OFFSET,
	DSS_RCH_VG0_PCSC_OFFSET,
	DSS_RCH_VG0_CSC_OFFSET,
	},

	/* G0 */
	{
	MIF_CH3_OFFSET,
	AIF0_CH3_OFFSET,
	AIF1_CH3_OFFSET,
	MCTL_CTL_MUTEX_RCH3,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH3_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH3_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH3_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD3_DBG,
	DSS_RCH_G0_DMA_OFFSET,
	DSS_RCH_G0_DFC_OFFSET,
	DSS_RCH_G0_SCL_OFFSET,
	0,
	0,
	0,
	DSS_RCH_G0_POST_CLIP_OFFSET,
	0,
	DSS_RCH_G0_CSC_OFFSET,
	},

	/* V1 */
	{
	MIF_CH4_OFFSET,
	AIF0_CH4_OFFSET,
	AIF1_CH4_OFFSET,
	MCTL_CTL_MUTEX_RCH4,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH4_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH4_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH4_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD4_DBG,
	DSS_RCH_VG1_DMA_OFFSET,
	DSS_RCH_VG1_DFC_OFFSET,
	DSS_RCH_VG1_SCL_OFFSET,
	DSS_RCH_VG1_SCL_LUT_OFFSET,
	0,
	0,
	DSS_RCH_VG1_POST_CLIP_OFFSET,
	0,
	DSS_RCH_VG1_CSC_OFFSET,
	},

	/* G1 */
	{
	MIF_CH5_OFFSET,
	AIF0_CH5_OFFSET,
	AIF1_CH5_OFFSET,
	MCTL_CTL_MUTEX_RCH5,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH5_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH5_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH5_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD5_DBG,
	DSS_RCH_G1_DMA_OFFSET,
	DSS_RCH_G1_DFC_OFFSET,
	DSS_RCH_G1_SCL_OFFSET,
	0,
	0,
	0,
	DSS_RCH_G1_POST_CLIP_OFFSET,
	0,
	DSS_RCH_G1_CSC_OFFSET,
	},

	/* D2 */
	{
	MIF_CH6_OFFSET,
	AIF0_CH6_OFFSET,
	AIF1_CH6_OFFSET,
	MCTL_CTL_MUTEX_RCH6,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH6_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH6_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH6_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD6_DBG,
	DSS_RCH_D2_DMA_OFFSET,
	DSS_RCH_D2_DFC_OFFSET,
	0,
	0,
	0,
	0,
	0,
	0,
	DSS_RCH_D2_CSC_OFFSET,
	},

	/* D3 */
	{
	MIF_CH7_OFFSET,
	AIF0_CH7_OFFSET,
	AIF1_CH7_OFFSET,
	MCTL_CTL_MUTEX_RCH7,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH7_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH7_OV_OEN,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH7_STARTY,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD7_DBG,
	DSS_RCH_D3_DMA_OFFSET,
	DSS_RCH_D3_DFC_OFFSET,
	0,
	0,
	0,
	0,
	0,
	0,
	DSS_RCH_D3_CSC_OFFSET,
	},

	/* W0 */
	{
	MIF_CH8_OFFSET,
	AIF0_CH8_OFFSET,
	AIF1_CH8_OFFSET,
	MCTL_CTL_MUTEX_WCH0,
	DSS_MCTRL_SYS_OFFSET + MCTL_WCH0_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_WCH0_OV_IEN,
	0,
	0,
	DSS_WCH0_DMA_OFFSET,
	DSS_WCH0_DFC_OFFSET,
	0,
	0,
	0,
	0,
	0,
	0,
	DSS_WCH0_CSC_OFFSET,
	},

	/* W1 */
	{
	MIF_CH9_OFFSET,
	AIF0_CH9_OFFSET,
	AIF1_CH9_OFFSET,
	MCTL_CTL_MUTEX_WCH1,
	DSS_MCTRL_SYS_OFFSET + MCTL_WCH1_FLUSH_EN,
	DSS_MCTRL_SYS_OFFSET + MCTL_WCH1_OV_IEN,
	0,
	0,
	DSS_WCH1_DMA_OFFSET,
	DSS_WCH1_DFC_OFFSET,
	0,
	0,
	0,
	0,
	0,
	0,
	DSS_WCH1_CSC_OFFSET,
	},
	/* V2 */
	{
	MIF_CH10_OFFSET,
	AIF0_CH11_OFFSET,
	AIF1_CH11_OFFSET,
	MCTL_CTL_MUTEX_RCH8,
	DSS_MCTRL_SYS_OFFSET + MCTL_RCH8_FLUSH_EN,
	0,
	0,
	DSS_MCTRL_SYS_OFFSET + MCTL_MOD8_DBG,
	DSS_RCH_VG2_DMA_OFFSET,
	DSS_RCH_VG2_DFC_OFFSET,
	DSS_RCH_VG2_SCL_OFFSET,
	DSS_RCH_VG2_SCL_LUT_OFFSET,
	0,
	0,
	DSS_RCH_VG2_POST_CLIP_OFFSET,
	0,
	DSS_RCH_VG2_CSC_OFFSET,
	},
	/* W2 */
	{
	MIF_CH11_OFFSET,
	AIF0_CH12_OFFSET,
	AIF1_CH12_OFFSET,
	MCTL_CTL_MUTEX_WCH2,
	DSS_MCTRL_SYS_OFFSET + MCTL_WCH2_FLUSH_EN,
	0,
	0,
	0,
	DSS_WCH2_DMA_OFFSET,
	DSS_WCH2_DFC_OFFSET,
	0,
	0,
	0,
	0,
	0,
	0,
	DSS_WCH2_CSC_OFFSET,
	},
};

/*lint +e785*/
u32 g_dss_module_ovl_base[DSS_MCTL_IDX_MAX][MODULE_OVL_MAX] = {
	{DSS_OVL0_OFFSET,
	DSS_MCTRL_CTL0_OFFSET},

	{DSS_OVL1_OFFSET,
	DSS_MCTRL_CTL1_OFFSET},

	{DSS_OVL2_OFFSET,
	DSS_MCTRL_CTL2_OFFSET},

	{DSS_OVL3_OFFSET,
	DSS_MCTRL_CTL3_OFFSET},

	{0,
	DSS_MCTRL_CTL4_OFFSET},

	{0,
	DSS_MCTRL_CTL5_OFFSET},
};

/*SCF_LUT_CHN coef_idx*/
int g_scf_lut_chn_coef_idx[DSS_CHN_MAX_DEFINE] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

u32 g_dss_module_cap[DSS_CHN_MAX_DEFINE][MODULE_CAP_MAX] = {
	/* D2 */
	{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
	/* D3 */
	{0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
	/* V0 */
	{0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1},
	/* G0 */
	{0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0},
	/* V1 */
	{0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1},
	/* G1 */
	{0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0},
	/* D0 */
	{0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
	/* D1 */
	{0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},

	/* W0 */
	{1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1},
	/* W1 */
	{1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1},

	/* V2 */
	{0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1},
	/* W2 */
	{1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1},
};

/* number of smrx idx for each channel */
u32 g_dss_chn_sid_num[DSS_CHN_MAX_DEFINE] = {
	4, 1, 4, 4, 4, 4, 1, 1, 3, 3, 3, 2
};

/* start idx of each channel */
/* smrx_idx = g_dss_smmu_smrx_idx[chn_idx] + (0 ~ g_dss_chn_sid_num[chn_idx]) */
u32 g_dss_smmu_smrx_idx[DSS_CHN_MAX_DEFINE] = {
	0, 4, 5, 9, 13, 17, 21, 22, 26, 29, 23, 32
};
u32 g_dss_mif_sid_map[DSS_CHN_MAX] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int hisi_pixel_format_hal2dma(int format)
{
	int ret = 0;

	switch(format) {
	case HISI_FB_PIXEL_FORMAT_RGB_565:
	case HISI_FB_PIXEL_FORMAT_BGR_565:
		ret = DMA_PIXEL_FORMAT_RGB_565;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBX_4444:
	case HISI_FB_PIXEL_FORMAT_BGRX_4444:
		ret = DMA_PIXEL_FORMAT_XRGB_4444;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBA_4444:
	case HISI_FB_PIXEL_FORMAT_BGRA_4444:
		ret = DMA_PIXEL_FORMAT_ARGB_4444;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBX_5551:
	case HISI_FB_PIXEL_FORMAT_BGRX_5551:
		ret = DMA_PIXEL_FORMAT_XRGB_5551;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBA_5551:
	case HISI_FB_PIXEL_FORMAT_BGRA_5551:
		ret = DMA_PIXEL_FORMAT_ARGB_5551;
		break;

	case HISI_FB_PIXEL_FORMAT_RGBX_8888:
	case HISI_FB_PIXEL_FORMAT_BGRX_8888:
		ret = DMA_PIXEL_FORMAT_XRGB_8888;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBA_8888:
	case HISI_FB_PIXEL_FORMAT_BGRA_8888:
		ret = DMA_PIXEL_FORMAT_ARGB_8888;
		break;

	case HISI_FB_PIXEL_FORMAT_YUV_422_I:
	case HISI_FB_PIXEL_FORMAT_YUYV_422_Pkg:
	case HISI_FB_PIXEL_FORMAT_YVYU_422_Pkg:
	case HISI_FB_PIXEL_FORMAT_UYVY_422_Pkg:
	case HISI_FB_PIXEL_FORMAT_VYUY_422_Pkg:
		ret = DMA_PIXEL_FORMAT_YUYV_422_Pkg;
		break;

	case HISI_FB_PIXEL_FORMAT_YCbCr_422_P:
	case HISI_FB_PIXEL_FORMAT_YCrCb_422_P:
		ret = DMA_PIXEL_FORMAT_YUV_422_P_HP;
		break;
	case HISI_FB_PIXEL_FORMAT_YCbCr_420_P:
	case HISI_FB_PIXEL_FORMAT_YCrCb_420_P:
		ret = DMA_PIXEL_FORMAT_YUV_420_P_HP;
		break;

	case HISI_FB_PIXEL_FORMAT_YCbCr_422_SP:
	case HISI_FB_PIXEL_FORMAT_YCrCb_422_SP:
		ret = DMA_PIXEL_FORMAT_YUV_422_SP_HP;
		break;
	case HISI_FB_PIXEL_FORMAT_YCbCr_420_SP:
	case HISI_FB_PIXEL_FORMAT_YCrCb_420_SP:
		ret = DMA_PIXEL_FORMAT_YUV_420_SP_HP;
		break;

	default:
		DRM_ERROR("not support format(%d)!\n", format);
		ret = -1;
		break;
	}

	return ret;
}

static int hisi_pixel_format_hal2dfc(int format)
{
	int ret = 0;

	switch (format) {
	case HISI_FB_PIXEL_FORMAT_RGB_565:
		ret = DFC_PIXEL_FORMAT_RGB_565;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBX_4444:
		ret = DFC_PIXEL_FORMAT_XBGR_4444;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBA_4444:
		ret = DFC_PIXEL_FORMAT_ABGR_4444;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBX_5551:
		ret = DFC_PIXEL_FORMAT_XBGR_5551;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBA_5551:
		ret = DFC_PIXEL_FORMAT_ABGR_5551;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBX_8888:
		ret = DFC_PIXEL_FORMAT_XBGR_8888;
		break;
	case HISI_FB_PIXEL_FORMAT_RGBA_8888:
		ret = DFC_PIXEL_FORMAT_ABGR_8888;
		break;

	case HISI_FB_PIXEL_FORMAT_BGR_565:
		ret = DFC_PIXEL_FORMAT_BGR_565;
		break;
	case HISI_FB_PIXEL_FORMAT_BGRX_4444:
		ret = DFC_PIXEL_FORMAT_XRGB_4444;
		break;
	case HISI_FB_PIXEL_FORMAT_BGRA_4444:
		ret = DFC_PIXEL_FORMAT_ARGB_4444;
		break;
	case HISI_FB_PIXEL_FORMAT_BGRX_5551:
		ret = DFC_PIXEL_FORMAT_XRGB_5551;
		break;
	case HISI_FB_PIXEL_FORMAT_BGRA_5551:
		ret = DFC_PIXEL_FORMAT_ARGB_5551;
		break;
	case HISI_FB_PIXEL_FORMAT_BGRX_8888:
		ret = DFC_PIXEL_FORMAT_XRGB_8888;
		break;
	case HISI_FB_PIXEL_FORMAT_BGRA_8888:
		ret = DFC_PIXEL_FORMAT_ARGB_8888;
		break;

	case HISI_FB_PIXEL_FORMAT_YUV_422_I:
	case HISI_FB_PIXEL_FORMAT_YUYV_422_Pkg:
		ret = DFC_PIXEL_FORMAT_YUYV422;
		break;
	case HISI_FB_PIXEL_FORMAT_YVYU_422_Pkg:
		ret = DFC_PIXEL_FORMAT_YVYU422;
		break;
	case HISI_FB_PIXEL_FORMAT_UYVY_422_Pkg:
		ret = DFC_PIXEL_FORMAT_UYVY422;
		break;
	case HISI_FB_PIXEL_FORMAT_VYUY_422_Pkg:
		ret = DFC_PIXEL_FORMAT_VYUY422;
		break;

	case HISI_FB_PIXEL_FORMAT_YCbCr_422_SP:
		ret = DFC_PIXEL_FORMAT_YUYV422;
		break;
	case HISI_FB_PIXEL_FORMAT_YCrCb_422_SP:
		ret = DFC_PIXEL_FORMAT_YVYU422;
		break;
	case HISI_FB_PIXEL_FORMAT_YCbCr_420_SP:
		ret = DFC_PIXEL_FORMAT_YUYV422;
		break;
	case HISI_FB_PIXEL_FORMAT_YCrCb_420_SP:
		ret = DFC_PIXEL_FORMAT_YVYU422;
		break;

	case HISI_FB_PIXEL_FORMAT_YCbCr_422_P:
	case HISI_FB_PIXEL_FORMAT_YCbCr_420_P:
		ret = DFC_PIXEL_FORMAT_YUYV422;
		break;
	case HISI_FB_PIXEL_FORMAT_YCrCb_422_P:
	case HISI_FB_PIXEL_FORMAT_YCrCb_420_P:
		ret = DFC_PIXEL_FORMAT_YVYU422;
		break;

	default:
		DRM_ERROR("not support format(%d)!\n", format);
		ret = -1;
		break;
	}

	return ret;
}

static int hisi_dss_aif_ch_config(struct dss_hw_ctx *ctx, int chn_idx)
{
	void __iomem *aif0_ch_base;
	int mid = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	mid = mid_array[chn_idx];
	aif0_ch_base = ctx->base + g_dss_module_base[chn_idx][MODULE_AIF0_CHN];

	set_reg(aif0_ch_base, 0x0, 1, 0);
	set_reg(aif0_ch_base, (uint32_t)mid, 4, 4);

	return 0;
}

static int hisi_dss_smmu_config(struct dss_hw_ctx *ctx, int chn_idx, bool mmu_enable)
{
	void __iomem *smmu_base;
	u32 idx = 0, i = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	smmu_base = ctx->base + DSS_SMMU_OFFSET;

	for (i = 0; i < g_dss_chn_sid_num[chn_idx]; i++) {
		idx = g_dss_smmu_smrx_idx[chn_idx] + i;
		if (!mmu_enable)
			set_reg(smmu_base + SMMU_SMRx_NS + idx * 0x4, 1, 32, 0);
		else
			set_reg(smmu_base + SMMU_SMRx_NS + idx * 0x4, 0x70, 32, 0);
	}

	return 0;
}

static int hisi_dss_mif_config(struct dss_hw_ctx *ctx, int chn_idx, bool mmu_enable)
{
	void __iomem *mif_base;
	void __iomem *mif_ch_base;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	mif_base = ctx->base + DSS_MIF_OFFSET;
	mif_ch_base = ctx->base +
		g_dss_module_base[chn_idx][MODULE_MIF_CHN];

	if (!mmu_enable) {
		set_reg(mif_ch_base + MIF_CTRL1, 0x1, 1, 5);
	} else  {
		set_reg(mif_ch_base + MIF_CTRL1, 0x00080000, 32, 0);
	}

	return 0;
}

int hisi_dss_mctl_mutex_lock(struct dss_hw_ctx *ctx)
{
	void __iomem *mctl_base;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	mctl_base = ctx->base +
		g_dss_module_ovl_base[DSS_OVL0][MODULE_MCTL_BASE];

	set_reg(mctl_base + MCTL_CTL_MUTEX, 0x1, 1, 0);

	return 0;
}

int hisi_dss_mctl_mutex_unlock(struct dss_hw_ctx *ctx)
{
	void __iomem *mctl_base;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	mctl_base = ctx->base +
		g_dss_module_ovl_base[DSS_OVL0][MODULE_MCTL_BASE];

	set_reg(mctl_base + MCTL_CTL_MUTEX, 0x0, 1, 0);

	return 0;
}

static int hisi_dss_mctl_ov_config(struct dss_hw_ctx *ctx, int chn_idx)
{
	void __iomem *mctl_base;
	u32 mctl_rch_offset = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	mctl_rch_offset = (uint32_t)(MCTL_CTL_MUTEX_RCH0 + chn_idx * 0x4);

	mctl_base = ctx->base +
		g_dss_module_ovl_base[DSS_OVL0][MODULE_MCTL_BASE];

	set_reg(mctl_base + MCTL_CTL_EN, 0x1, 32, 0);
	set_reg(mctl_base + MCTL_CTL_TOP, 0x2, 32, 0); /*auto mode*/
	set_reg(mctl_base + MCTL_CTL_DBG, 0xB13A00, 32, 0);

	set_reg(mctl_base + mctl_rch_offset, 0x1, 32, 0);
	set_reg(mctl_base + MCTL_CTL_MUTEX_ITF, 0x1, 2, 0);
	set_reg(mctl_base + MCTL_CTL_MUTEX_DBUF, 0x1, 2, 0);
	set_reg(mctl_base + MCTL_CTL_MUTEX_OV, 1 << DSS_OVL0, 4, 0);

	return 0;
}

static int hisi_dss_mctl_sys_config(struct dss_hw_ctx *ctx, int chn_idx)
{
	void __iomem *mctl_sys_base;

	u32 layer_idx = 0;
	u32 mctl_rch_ov_oen_offset = 0;
	u32 mctl_rch_flush_en_offset = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	mctl_sys_base = ctx->base + DSS_MCTRL_SYS_OFFSET;
	mctl_rch_ov_oen_offset = MCTL_RCH0_OV_OEN + chn_idx * 0x4;
	mctl_rch_flush_en_offset = MCTL_RCH0_FLUSH_EN + chn_idx * 0x4;

	set_reg(mctl_sys_base + mctl_rch_ov_oen_offset,
		((1 << (layer_idx + 1)) | (0x100 << DSS_OVL0)), 32, 0);

	set_reg(mctl_sys_base + MCTL_RCH_OV0_SEL, 0x8, 4, 0);

	set_reg(mctl_sys_base + MCTL_RCH_OV0_SEL, chn_idx, 4, (layer_idx + 1) * 4);

	set_reg(mctl_sys_base + MCTL_OV0_FLUSH_EN, 0xd, 4, 0);
	set_reg(mctl_sys_base + mctl_rch_flush_en_offset, 0x1, 32, 0);

	return 0;
}

static int hisi_dss_rdma_config(struct dss_hw_ctx *ctx,
	const dss_rect_ltrb_t *rect, u32 display_addr, u32 hal_format,
	u32 bpp, int chn_idx, bool afbcd, bool mmu_enable)
{
	void __iomem *rdma_base;

	u32 aligned_pixel = 0;
	u32 rdma_oft_x0 = 0;
	u32 rdma_oft_y0 = 0;
	u32 rdma_oft_x1 = 0;
	u32 rdma_oft_y1 = 0;
	u32 rdma_stride = 0;
	u32 rdma_bpp = 0;
	u32 rdma_format = 0;
	u32 stretch_size_vrt = 0;

	u32 stride_align = 0;
	u32 mm_base_0 = 0;
	u32 mm_base_1 = 0;

	u32 afbcd_header_addr = 0;
	u32 afbcd_header_stride = 0;
	u32 afbcd_payload_addr = 0;
	u32 afbcd_payload_stride = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	if (bpp == 4) {
		rdma_bpp = 0x5;
	} else if (bpp == 2) {
		rdma_bpp = 0x0;
	} else {
		rdma_bpp = 0x0;
	}

	rdma_base = ctx->base +
		g_dss_module_base[chn_idx][MODULE_DMA];

	aligned_pixel = DMA_ALIGN_BYTES / bpp;
	rdma_oft_x0 = rect->left / aligned_pixel;
	rdma_oft_y0 = rect->top;
	rdma_oft_x1 = rect->right / aligned_pixel;
	rdma_oft_y1 = rect->bottom;

	rdma_format = hisi_pixel_format_hal2dma(hal_format);
	if (rdma_format < 0) {
		DRM_ERROR("layer format(%d) not support !\n", hal_format);
		return -EINVAL;
	}

	if (afbcd) {
		mm_base_0 = 0;
		mm_base_1 = mm_base_0 + rect->right * bpp * MMBUF_LINE_NUM;
		mm_base_0 = ALIGN_UP(mm_base_0, MMBUF_ADDR_ALIGN);
		mm_base_1 = ALIGN_UP(mm_base_1, MMBUF_ADDR_ALIGN);

		if ((((rect->right - rect->left) + 1) & (AFBC_HEADER_ADDR_ALIGN - 1)) ||
				(((rect->bottom - rect->top) + 1) & (AFBC_BLOCK_ALIGN - 1))) {
			DRM_ERROR("img width(%d) is not %d bytes aligned, or "
					"img heigh(%d) is not %d bytes aligned!\n",
					((rect->right - rect->left) + 1), AFBC_HEADER_ADDR_ALIGN,
					((rect->bottom - rect->top) + 1), AFBC_BLOCK_ALIGN);
		}

		if ((mm_base_0 & (MMBUF_ADDR_ALIGN - 1)) || (mm_base_1 & (MMBUF_ADDR_ALIGN - 1))) {
			DRM_ERROR("mm_base_0(0x%x) is not %d bytes aligned, or "
					"mm_base_1(0x%x) is not %d bytes aligned!\n",
					mm_base_0, MMBUF_ADDR_ALIGN,
					mm_base_1, MMBUF_ADDR_ALIGN);
		}
		/*header*/
		afbcd_header_stride = (((rect->right - rect->left) + 1) / AFBC_BLOCK_ALIGN) * AFBC_HEADER_STRIDE_BLOCK;
		afbcd_header_addr = (uint32_t)(unsigned long)display_addr;

		/*payload*/
		if (bpp == 4)
			stride_align = AFBC_PAYLOAD_STRIDE_ALIGN_32;
		else if (bpp == 2)
			stride_align = AFBC_PAYLOAD_STRIDE_ALIGN_16;
		else
			DRM_ERROR("bpp(%d) not supported!\n", bpp);

		afbcd_payload_stride = (((rect->right - rect->left) + 1) / AFBC_BLOCK_ALIGN) * stride_align;

		afbcd_payload_addr = afbcd_header_addr + ALIGN_UP(16 * (((rect->right - rect->left) + 1) / 16) *
				(((rect->bottom - rect->top) + 1) / 16), 1024);
		afbcd_payload_addr = afbcd_payload_addr +
			(rect->top / AFBC_BLOCK_ALIGN) * afbcd_payload_stride +
			(rect->left / AFBC_BLOCK_ALIGN) * stride_align;

		set_reg(rdma_base + CH_REG_DEFAULT, 0x1, 32, 0);
		set_reg(rdma_base + CH_REG_DEFAULT, 0x0, 32, 0);
		set_reg(rdma_base + DMA_OFT_X0, rdma_oft_x0, 12, 0);
		set_reg(rdma_base + DMA_OFT_Y0, rdma_oft_y0, 16, 0);
		set_reg(rdma_base + DMA_OFT_X1, rdma_oft_x1, 12, 0);
		set_reg(rdma_base + DMA_OFT_Y1, rdma_oft_y1, 16, 0);
		set_reg(rdma_base + DMA_STRETCH_SIZE_VRT, (rect->bottom - rect->top), 13, 0);
		set_reg(rdma_base + DMA_CTRL, rdma_format, 5, 3);
		set_reg(rdma_base + DMA_CTRL, (mmu_enable ? 0x1 : 0x0), 1, 8);

		set_reg(rdma_base + AFBCD_HREG_PIC_WIDTH, (rect->right - rect->left), 16, 0);
		set_reg(rdma_base + AFBCD_HREG_PIC_HEIGHT, (rect->bottom - rect->top), 16, 0);
		set_reg(rdma_base + AFBCD_CTL, AFBC_HALF_BLOCK_UPPER_LOWER_ALL, 2, 6);
		set_reg(rdma_base + AFBCD_HREG_HDR_PTR_LO, afbcd_header_addr, 32, 0);
		set_reg(rdma_base + AFBCD_INPUT_HEADER_STRIDE, afbcd_header_stride, 14, 0);
		set_reg(rdma_base + AFBCD_PAYLOAD_STRIDE, afbcd_payload_stride, 20, 0);
		set_reg(rdma_base + AFBCD_MM_BASE_0, mm_base_0, 32, 0);
		set_reg(rdma_base + AFBCD_HREG_FORMAT, 0x1, 1, 21);
		set_reg(rdma_base + AFBCD_SCRAMBLE_MODE, 0x0, 32, 0);
		set_reg(rdma_base + AFBCD_AFBCD_PAYLOAD_POINTER, afbcd_payload_addr, 32, 0);
		set_reg(rdma_base + AFBCD_HEIGHT_BF_STR, (rect->bottom - rect->top), 16, 0);

		set_reg(rdma_base + CH_CTL, 0xf005, 32, 0);
	} else {
		stretch_size_vrt = rdma_oft_y1 - rdma_oft_y0;
		rdma_stride = ((rect->right - rect->left) + 1) * bpp / DMA_ALIGN_BYTES;

		set_reg(rdma_base + CH_REG_DEFAULT, 0x1, 32, 0);
		set_reg(rdma_base + CH_REG_DEFAULT, 0x0, 32, 0);

		set_reg(rdma_base + DMA_OFT_X0, rdma_oft_x0, 12, 0);
		set_reg(rdma_base + DMA_OFT_Y0, rdma_oft_y0, 16, 0);
		set_reg(rdma_base + DMA_OFT_X1, rdma_oft_x1, 12, 0);
		set_reg(rdma_base + DMA_OFT_Y1, rdma_oft_y1, 16, 0);
		set_reg(rdma_base + DMA_CTRL, rdma_format, 5, 3);
		set_reg(rdma_base + DMA_CTRL, (mmu_enable ? 0x1 : 0x0), 1, 8);
		set_reg(rdma_base + DMA_STRETCH_SIZE_VRT, stretch_size_vrt, 32, 0);
		set_reg(rdma_base + DMA_DATA_ADDR0, display_addr, 32, 0);
		set_reg(rdma_base + DMA_STRIDE0, rdma_stride, 13, 0);

		set_reg(rdma_base + CH_CTL, 0x1, 1, 0);
	}

	return 0;
}

static int hisi_dss_rdfc_config(struct dss_hw_ctx *ctx,
	const dss_rect_ltrb_t *rect, u32 hal_format, u32 bpp, int chn_idx)
{
	void __iomem *rdfc_base;

	u32 dfc_pix_in_num = 0;
	u32 size_hrz = 0;
	u32 size_vrt = 0;
	u32 dfc_fmt = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	rdfc_base = ctx->base +
		g_dss_module_base[chn_idx][MODULE_DFC];

	dfc_pix_in_num = (bpp <= 2) ? 0x1 : 0x0;
	size_hrz = rect->right - rect->left;
	size_vrt = rect->bottom - rect->top;

	dfc_fmt = hisi_pixel_format_hal2dfc(hal_format);
	if (dfc_fmt < 0) {
		DRM_ERROR("layer format (%d) not support !\n", hal_format);
		return -EINVAL;
	}

	set_reg(rdfc_base + DFC_DISP_SIZE, (size_vrt | (size_hrz << 16)), 29, 0);
	set_reg(rdfc_base + DFC_PIX_IN_NUM, dfc_pix_in_num, 1, 0);
	//set_reg(rdfc_base + DFC_DISP_FMT, (bpp <= 2) ? 0x0 : 0x6, 5, 1);
	set_reg(rdfc_base + DFC_DISP_FMT, dfc_fmt, 5, 1);
	set_reg(rdfc_base + DFC_CTL_CLIP_EN, 0x1, 1, 0);
	set_reg(rdfc_base + DFC_ICG_MODULE, 0x1, 1, 0);

	return 0;
}

int hisi_dss_ovl_base_config(struct dss_hw_ctx *ctx, u32 xres, u32 yres)
{
	void __iomem *mctl_sys_base;
	void __iomem *mctl_base;
	void __iomem *ovl0_base;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	mctl_sys_base = ctx->base + DSS_MCTRL_SYS_OFFSET;
	mctl_base = ctx->base +
		g_dss_module_ovl_base[DSS_OVL0][MODULE_MCTL_BASE];
	ovl0_base = ctx->base +
		g_dss_module_ovl_base[DSS_OVL0][MODULE_OVL_BASE];

	set_reg(ovl0_base + OVL6_REG_DEFAULT, 0x1, 32, 0);
	set_reg(ovl0_base + OVL6_REG_DEFAULT, 0x0, 32, 0);

	set_reg(ovl0_base + OVL_SIZE, (xres - 1) | ((yres - 1) << 16), 32, 0);
#ifdef CONFIG_HISI_FB_OV_BASE_USED
	set_reg(ovl0_base + OVL_BG_COLOR, 0xFFFF0000, 32, 0);
#else
	set_reg(ovl0_base + OVL_BG_COLOR, 0xFF000000, 32, 0);
#endif
	set_reg(ovl0_base + OVL_DST_STARTPOS, 0x0, 32, 0);
	set_reg(ovl0_base + OVL_DST_ENDPOS, (xres - 1) | ((yres - 1) << 16), 32, 0);
	set_reg(ovl0_base + OVL_GCFG, 0x10001, 32, 0);

	set_reg(mctl_base + MCTL_CTL_MUTEX_ITF, 0x1, 32, 0);
	set_reg(mctl_base + MCTL_CTL_MUTEX_DBUF, 0x1, 2, 0);
	set_reg(mctl_base + MCTL_CTL_MUTEX_OV, 1 << DSS_OVL0, 4, 0);

	set_reg(mctl_sys_base + MCTL_RCH_OV0_SEL, 0x8, 4, 0);
	set_reg(mctl_sys_base + MCTL_OV0_FLUSH_EN, 0xd, 4, 0);

	return 0;
}

static int hisi_dss_ovl_config(struct dss_hw_ctx *ctx,
	const dss_rect_ltrb_t *rect, u32 xres, u32 yres)
{
	void __iomem *ovl0_base;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return -1;
	}

	ovl0_base = ctx->base +
		g_dss_module_ovl_base[DSS_OVL0][MODULE_OVL_BASE];

	set_reg(ovl0_base + OVL6_REG_DEFAULT, 0x1, 32, 0);
	set_reg(ovl0_base + OVL6_REG_DEFAULT, 0x0, 32, 0);
	set_reg(ovl0_base + OVL_SIZE, (xres - 1) |
		((yres - 1) << 16), 32, 0);
	set_reg(ovl0_base + OVL_BG_COLOR, 0xFF000000, 32, 0);
	set_reg(ovl0_base + OVL_DST_STARTPOS, 0x0, 32, 0);
	set_reg(ovl0_base + OVL_DST_ENDPOS, (xres - 1) |
		((yres - 1) << 16), 32, 0);
	set_reg(ovl0_base + OVL_GCFG, 0x10001, 32, 0);
	set_reg(ovl0_base + OVL_LAYER0_POS, (rect->left) |
		((rect->top) << 16), 32, 0);
	set_reg(ovl0_base + OVL_LAYER0_SIZE, (rect->right) |
		((rect->bottom) << 16), 32, 0);
	set_reg(ovl0_base + OVL_LAYER0_ALPHA, 0x00ff40ff, 32, 0);
	set_reg(ovl0_base + OVL_LAYER0_CFG, 0x1, 1, 0);

	return 0;
}

static void hisi_dss_qos_on(struct dss_hw_ctx *ctx)
{
	char __iomem *noc_dss_base;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	noc_dss_base = ctx->noc_dss_base;

	outp32(noc_dss_base + 0xc, 0x2);
	outp32(noc_dss_base + 0x8c, 0x2);
	outp32(noc_dss_base + 0x10c, 0x2);
	outp32(noc_dss_base + 0x18c, 0x2);
}

static void hisi_dss_mif_on(struct dss_hw_ctx *ctx)
{
	char __iomem *dss_base;
	char __iomem *mif_base;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	dss_base = ctx->base;
	mif_base = ctx->base + DSS_MIF_OFFSET;

	set_reg(mif_base + MIF_ENABLE, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH0_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH1_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH2_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH3_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH4_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH5_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH6_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH7_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH8_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH9_OFFSET + MIF_CTRL0, 0x1, 1, 0);

	set_reg(dss_base + MIF_CH10_OFFSET + MIF_CTRL0, 0x1, 1, 0);
	set_reg(dss_base + MIF_CH11_OFFSET + MIF_CTRL0, 0x1, 1, 0);
}

void hisi_dss_smmu_on(struct dss_hw_ctx *ctx)
{
	void __iomem *smmu_base;
	struct iommu_domain_data *domain_data = NULL;
	uint32_t phy_pgd_base = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	smmu_base = ctx->base + DSS_SMMU_OFFSET;

	set_reg(smmu_base + SMMU_SCR, 0x0, 1, 0);  /*global bypass cancel*/
	set_reg(smmu_base + SMMU_SCR, 0x1, 8, 20); /*ptw_mid*/
	set_reg(smmu_base + SMMU_SCR, 0xf, 4, 16); /*pwt_pf*/
	set_reg(smmu_base + SMMU_SCR, 0x7, 3, 3);  /*interrupt cachel1 cach3l2 en*/
	set_reg(smmu_base + SMMU_LP_CTRL, 0x1, 1, 0);  /*auto_clk_gt_en*/

	/*Long Descriptor*/
	set_reg(smmu_base + SMMU_CB_TTBCR, 0x1, 1, 0);

	set_reg(smmu_base + SMMU_ERR_RDADDR, 0x7FF00000, 32, 0);
	set_reg(smmu_base + SMMU_ERR_WRADDR, 0x7FFF0000, 32, 0);

	/*disable cmdlist, dbg, reload*/
	set_reg(smmu_base + SMMU_RLD_EN0_NS, DSS_SMMU_RLD_EN0_DEFAULT_VAL, 32, 0);
	set_reg(smmu_base + SMMU_RLD_EN1_NS, DSS_SMMU_RLD_EN1_DEFAULT_VAL, 32, 0);

	/*cmdlist stream bypass*/
	set_reg(smmu_base + SMMU_SMRx_NS + 36 * 0x4, 0x1, 32, 0); /*debug stream id*/
	set_reg(smmu_base + SMMU_SMRx_NS + 37 * 0x4, 0x1, 32, 0); /*cmd unsec stream id*/
	set_reg(smmu_base + SMMU_SMRx_NS + 38 * 0x4, 0x1, 32, 0); /*cmd sec stream id*/

	/*TTBR0*/
	domain_data = (struct iommu_domain_data *)(ctx->mmu_domain->priv);
	phy_pgd_base = (uint32_t)(domain_data->phy_pgd_base);
	set_reg(smmu_base + SMMU_CB_TTBR0, phy_pgd_base, 32, 0);
}

void hisifb_dss_on(struct dss_hw_ctx *ctx)
{
	/* dss qos on*/
	hisi_dss_qos_on(ctx);
	/* mif on*/
	hisi_dss_mif_on(ctx);
	/* smmu on*/
	hisi_dss_smmu_on(ctx);
}

void hisi_dss_mctl_on(struct dss_hw_ctx *ctx)
{
	char __iomem *mctl_base = NULL;
	char __iomem *mctl_sys_base = NULL;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}
	mctl_base = ctx->base +
		g_dss_module_ovl_base[DSS_MCTL0][MODULE_MCTL_BASE];
	mctl_sys_base = ctx->base + DSS_MCTRL_SYS_OFFSET;

	set_reg(mctl_base + MCTL_CTL_EN, 0x1, 32, 0);
	set_reg(mctl_base + MCTL_CTL_MUTEX_ITF, 0x1, 32, 0);
	set_reg(mctl_base + MCTL_CTL_DBG, 0xB13A00, 32, 0);
	set_reg(mctl_base + MCTL_CTL_TOP, 0x2, 32, 0);
}

void hisi_dss_unflow_handler(struct dss_hw_ctx *ctx, bool unmask)
{
	void __iomem *dss_base;
	u32 tmp = 0;

	if (!ctx) {
		DRM_ERROR("ctx is NULL!\n");
		return;
	}

	dss_base = ctx->base;

	tmp = inp32(dss_base + DSS_LDI0_OFFSET + LDI_CPU_ITF_INT_MSK);
	if (unmask)
		tmp &= ~BIT_LDI_UNFLOW;
	else
		tmp |= BIT_LDI_UNFLOW;

	outp32(dss_base + DSS_LDI0_OFFSET + LDI_CPU_ITF_INT_MSK, tmp);
}

static int hisi_dss_wait_for_complete(struct dss_hw_ctx *ctx)
{
	int ret = 0;
	u32 times = 0;
	u32 prev_vactive0_end = 0;

	prev_vactive0_end = ctx->vactive0_end_flag;

REDO:
	ret = wait_event_interruptible_timeout(ctx->vactive0_end_wq,
		(prev_vactive0_end != ctx->vactive0_end_flag),
		msecs_to_jiffies(300));
	if (ret == -ERESTARTSYS) {
		if (times < 50) {
			times++;
			mdelay(10);
			goto REDO;
		}
	}

	if (ret <= 0) {
		DRM_ERROR("wait_for vactive0_end_flag timeout! ret=%d.\n", ret);

		ret = -ETIMEDOUT;
	} else {
		ret = 0;
	}

	return ret;
}

void hisi_fb_pan_display(struct drm_plane *plane)
{
	struct drm_plane_state *state = plane->state;
	struct drm_framebuffer *fb = state->fb;
	struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;

	struct dss_plane *aplane = to_dss_plane(plane);
	struct dss_crtc *acrtc = aplane->acrtc;
	struct dss_hw_ctx *ctx = acrtc->ctx;

	struct kirin_drm_private *priv = plane->dev->dev_private;
	struct kirin_fbdev *fbdev = to_kirin_fbdev(priv->fbdev);

	bool afbcd = false;
	bool mmu_enable = true;
	dss_rect_ltrb_t rect;
	u32 bpp;
	u32 stride;
	u32 display_addr = 0;
	u32 hal_fmt;
	int chn_idx = DSS_RCHN_D2;

	int crtc_x = state->crtc_x;
	int crtc_y = state->crtc_y;
	unsigned int crtc_w = state->crtc_w;
	unsigned int crtc_h = state->crtc_h;
	u32 src_x = state->src_x >> 16;
	u32 src_y = state->src_y >> 16;
	u32 src_w = state->src_w >> 16;
	u32 src_h = state->src_h >> 16;

	u32 hfp, hbp, hsw, vfp, vbp, vsw;

	mode = &acrtc->base.state->mode;
	adj_mode = &acrtc->base.state->adjusted_mode;

	bpp = fb->bits_per_pixel / 8;
	stride = fb->pitches[0];

	if (fbdev)
		display_addr = (u32)fbdev->smem_start + src_y * stride;
	else
		printk("JDB: fbdev is null?\n");

	rect.left = 0;
	rect.right = src_w - 1;
	rect.top = 0;
	rect.bottom = src_h - 1;
	hal_fmt = dss_get_format(fb->pixel_format);

	DRM_DEBUG("channel%d: src:(%d,%d, %dx%d) crtc:(%d,%d, %dx%d), rect(%d,%d,%d,%d),"
		"fb:%dx%d, pixel_format=%d, stride=%d, paddr=0x%x, bpp=%d, bits_per_pixel=%d.\n",
		chn_idx, src_x, src_y, src_w, src_h,
		crtc_x, crtc_y, crtc_w, crtc_h,
		rect.left, rect.top, rect.right, rect.bottom,
		fb->width, fb->height, hal_fmt,
		stride, display_addr, bpp, fb->bits_per_pixel);

	hfp = mode->hsync_start - mode->hdisplay;
	hbp = mode->htotal - mode->hsync_end;
	hsw = mode->hsync_end - mode->hsync_start;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;
	vsw = mode->vsync_end - mode->vsync_start;

	hisi_dss_mctl_mutex_lock(ctx);
	hisi_dss_aif_ch_config(ctx, chn_idx);
	hisi_dss_mif_config(ctx, chn_idx, mmu_enable);
	hisi_dss_smmu_config(ctx, chn_idx, mmu_enable);

	hisi_dss_rdma_config(ctx, &rect, display_addr, hal_fmt, bpp, chn_idx, afbcd, mmu_enable);
	hisi_dss_rdfc_config(ctx, &rect, hal_fmt, bpp, chn_idx);
	hisi_dss_ovl_config(ctx, &rect, mode->hdisplay, mode->vdisplay);

	hisi_dss_mctl_ov_config(ctx, chn_idx);
	hisi_dss_mctl_sys_config(ctx, chn_idx);
	hisi_dss_mctl_mutex_unlock(ctx);
	hisi_dss_unflow_handler(ctx, true);

	enable_ldi(acrtc);
	hisi_dss_wait_for_complete(ctx);
}

void hisi_dss_online_play(struct drm_plane *plane, drm_dss_layer_t *layer)
{
	struct drm_plane_state *state = plane->state;
	struct drm_display_mode *mode;
	struct drm_display_mode *adj_mode;

	struct dss_plane *aplane = to_dss_plane(plane);
	struct dss_crtc *acrtc = aplane->acrtc;
	struct dss_hw_ctx *ctx = acrtc->ctx;

	bool afbcd = false;
	bool mmu_enable = true;
	dss_rect_ltrb_t rect;
	u32 bpp;
	u32 stride;
	u32 display_addr;

	int chn_idx = DSS_RCHN_D2;
	u32 hal_fmt = 0;
	u32 src_w = state->src_w >> 16;
	u32 src_h = state->src_h >> 16;

	u32 hfp, hbp, hsw, vfp, vbp, vsw;

	mode = &acrtc->base.state->mode;
	adj_mode = &acrtc->base.state->adjusted_mode;

	bpp = layer->img.bpp;
	stride = layer->img.stride;
	display_addr = layer->img.vir_addr;
	hal_fmt = layer->img.format;

	rect.left = 0;
	rect.right = src_w - 1;
	rect.top = 0;
	rect.bottom = src_h - 1;

	hfp = mode->hsync_start - mode->hdisplay;
	hbp = mode->htotal - mode->hsync_end;
	hsw = mode->hsync_end - mode->hsync_start;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;
	vsw = mode->vsync_end - mode->vsync_start;

	hisi_dss_mctl_mutex_lock(ctx);
	hisi_dss_aif_ch_config(ctx, chn_idx);
	hisi_dss_mif_config(ctx, chn_idx, mmu_enable);
	hisi_dss_smmu_config(ctx, chn_idx, mmu_enable);

	hisi_dss_rdma_config(ctx, &rect, display_addr, hal_fmt, bpp, chn_idx, afbcd, mmu_enable);
	hisi_dss_rdfc_config(ctx, &rect, hal_fmt, bpp, chn_idx);
	hisi_dss_ovl_config(ctx, &rect, mode->hdisplay, mode->vdisplay);

	hisi_dss_mctl_ov_config(ctx, chn_idx);
	hisi_dss_mctl_sys_config(ctx, chn_idx);
	hisi_dss_mctl_mutex_unlock(ctx);
	hisi_dss_unflow_handler(ctx, true);

	enable_ldi(acrtc);
	hisi_dss_wait_for_complete(ctx);
}

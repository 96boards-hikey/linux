/*
 * Copyright (c) 2014-2015 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HISI_ADE_REG_H__
#define __HISI_ADE_REG_H__

/*
 * ADE Registers Offset
 */
#define ADE_CTRL			(0x4)
#define ADE_CTRL1			(0x8C)
#define ADE_ROT_SRC_CFG			(0x10)
#define ADE_DISP_SRC_CFG		(0x18)
#define ADE_WDMA2_SRC_CFG		(0x1C)
#define ADE_SEC_OVLY_SRC_CFG		(0x20)
#define ADE_WDMA3_SRC_CFG		(0x24)
#define ADE_OVLY1_TRANS_CFG		(0x2C)
#define ADE_EN				(0x100)
#define INTR_MASK_CPU_0			(0xC10)
#define INTR_MASK_CPU_1			(0xC14)
#define ADE_FRM_DISGARD_CTRL		(0xA4)
/* reset and reload regs */
#define ADE_SOFT_RST_SEL0		(0x78)
#define ADE_SOFT_RST_SEL1		(0x7C)
#define ADE_RELOAD_DIS0			(0xAC)
#define ADE_RELOAD_DIS1			(0xB0)
#define ADE_CH_RDMA_BIT_OFST		(0)
#define ADE_CLIP_BIT_OFST		(15)
#define ADE_SCL_BIT_OFST		(21)
#define ADE_CTRAN_BIT_OFST		(24)
#define ADE_OVLY_BIT_OFST		(37) /* 32+5 */
/* channel regs */
#define RD_CH_PE(x)			(0x1000 + (x) * 0x80)
#define RD_CH_CTRL(x)			(0x1004 + (x) * 0x80)
#define RD_CH_ADDR(x)			(0x1008 + (x) * 0x80)
#define RD_CH_SIZE(x)			(0x100C + (x) * 0x80)
#define RD_CH_STRIDE(x)			(0x1010 + (x) * 0x80)
#define RD_CH_SPACE(x)			(0x1014 + (x) * 0x80)
#define RD_CH_PARTIAL_SIZE(x)		(0x1018 + (x) * 0x80)
#define RD_CH_PARTIAL_SPACE(x)		(0x101C + (x) * 0x80)
#define RD_CH_EN(x)			(0x1020 + (x) * 0x80)
#define RD_CH_STATUS(x)			(0x1024 + (x) * 0x80)
#define RD_CH_DISP_CTRL			(0x1404)
#define RD_CH_DISP_ADDR			(0x1408)
#define RD_CH_DISP_SIZE			(0x140C)
#define RD_CH_DISP_STRIDE		(0x1410)
#define RD_CH_DISP_SPACE		(0x1414)
#define RD_CH_DISP_EN			(0x142C)
/* clip regs */
#define ADE_CLIP_DISABLE(x)		(0x6800 + (x) * 0x100)
#define ADE_CLIP_SIZE0(x)		(0x6804 + (x) * 0x100)
#define ADE_CLIP_SIZE1(x)		(0x6808 + (x) * 0x100)
#define ADE_CLIP_SIZE2(x)		(0x680C + (x) * 0x100)
#define ADE_CLIP_CFG_OK(x)		(0x6810 + (x) * 0x100)
/* scale regs */
#define ADE_SCL1_MUX_CFG		(0xC)
#define ADE_SCL2_SRC_CFG		(0x14)
#define ADE_SCL3_MUX_CFG		(0x8)
#define ADE_SCL_CTRL(x)			(0x3000 + (x) * 0x800)
#define ADE_SCL_HSP(x)			(0x3004 + (x) * 0x800)
#define ADE_SCL_UV_HSP(x)		(0x3008 + (x) * 0x800)
#define ADE_SCL_VSP(x)			(0x300C + (x) * 0x800)
#define ADE_SCL_UV_VSP(x)		(0x3010 + (x) * 0x800)
#define ADE_SCL_ORES(x)			(0x3014 + (x) * 0x800)
#define ADE_SCL_IRES(x)			(0x3018 + (x) * 0x800)
#define ADE_SCL_START(x)		(0x301C + (x) * 0x800)
#define ADE_SCL_ERR(x)			(0x3020 + (x) * 0x800)
#define ADE_SCL_PIX_OFST(x)		(0x3024 + (x) * 0x800)
#define ADE_SCL_UV_PIX_OFST(x)		(0x3028 + (x) * 0x800)
#define ADE_SCL_COEF_CLR(x)		(0x3030 + (x) * 0x800)
#define ADE_SCL_HCOEF(x, m, n)		(0x3100 + (x) * 0x800 + \
					12 * (m) + 4 * (n))
#define ADE_SCL_VCOEF(x, i, j)		(0x340C + (x) * 0x800 + \
					12 * (i) + 4 * (j))
/* ctran regs */
#define ADE_CTRAN5_TRANS_CFG		(0x40)
#define ADE_CTRAN_DIS(x)		(0x5004 + (x) * 0x100)
#define ADE_CTRAN_MODE_CHOOSE(x)	(0x5008 + (x) * 0x100)
#define ADE_CTRAN_STAT(x)		(0x500C + (x) * 0x100)
#define ADE_CTRAN_CHDC0(x)		(0x5010 + (x) * 0x100)
#define ADE_CTRAN_CHDC1(x)		(0x5014 + (x) * 0x100)
#define ADE_CTRAN_CHDC2(x)		(0x5018 + (x) * 0x100)
#define ADE_CTRAN_CHDC3(x)		(0x501C + (x) * 0x100)
#define ADE_CTRAN_CHDC4(x)		(0x5020 + (x) * 0x100)
#define ADE_CTRAN_CHDC5(x)		(0x5024 + (x) * 0x100)
#define ADE_CTRAN_CSC0(x)		(0x5028 + (x) * 0x100)
#define ADE_CTRAN_CSC1(x)		(0x502C + (x) * 0x100)
#define ADE_CTRAN_CSC2(x)		(0x5030 + (x) * 0x100)
#define ADE_CTRAN_CSC3(x)		(0x5034 + (x) * 0x100)
#define ADE_CTRAN_CSC4(x)		(0x5038 + (x) * 0x100)
#define ADE_CTRAN_IMAGE_SIZE(x)		(0x503C + (x) * 0x100)
#define ADE_CTRAN_CFG_OK(x)		(0x5040 + (x) * 0x100)
/* overlay regs */
#define ADE_OVLY_ALPHA_ST		(0x2000)
#define ADE_OVLY_CH_XY0(x)		(0x2004 + (x) * 4)
#define ADE_OVLY_CH_XY1(x)		(0x2024 + (x) * 4)
#define ADE_OVLY_CH_CTL(x)		(0x204C + (x) * 4)
#define ADE_OVLY_OUTPUT_SIZE(x)		(0x2070 + (x) * 8)
#define ADE_OVLY_BASE_COLOR(x)		(0x2074 + (x) * 8)
#define ADE_OVLYX_CTL(x)		(0x209C + (x) * 4)
#define ADE_OVLY_CTL			(0x98)
#define ADE_OVLY_CH_ALP_MODE_OFST	(0)
#define ADE_OVLY_CH_ALP_SEL_OFST	(2)
#define ADE_OVLY_CH_UNDER_ALP_SEL_OFST	(4)
#define ADE_OVLY_CH_EN_OFST		(6)
#define ADE_OVLY_CH_ALP_GBL_OFST	(15)
#define ADE_OVLY_CH_SEL_OFST		(28)

/*
 * media regs
 */
#define SC_MEDIA_RSTDIS			(0x530)
#define SC_MEDIA_RSTEN			(0x52C)
#define NOC_ADE0_QOSGENERATOR_MODE       0x010C
#define NOC_ADE0_QOSGENERATOR_EXTCONTROL 0x0118
#define NOC_ADE1_QOSGENERATOR_MODE       0x020C
#define NOC_ADE1_QOSGENERATOR_EXTCONTROL 0x0218

/*
 * regs relevant enum
 */
enum {
	LDI_TEST = 0,
	LDI_WORK
};

enum {
	LDI_ISR_FRAME_END_INT = 0x2,
	LDI_ISR_UNDER_FLOW_INT = 0x4
};

enum {
	ADE_ISR1_RES_SWITCH_CMPL = 0x80000000
};

enum {
	LDI_DISP_MODE_NOT_3D_FBF = 0,
	LDI_DISP_MODE_3D_FBF
};

enum {
	ADE_RGB = 0,
	ADE_BGR
};

enum {
	ADE_DISABLE = 0,
	ADE_ENABLE
};

enum {
	ADE_OUT_RGB_565 = 0,
	ADE_OUT_RGB_666,
	ADE_OUT_RGB_888
};

/*
 * ADE read as big-endian, so revert the
 * rgb order described in the SoC datasheet
 */
enum ADE_FORMAT {
	ADE_RGB_565 = 0,
	ADE_BGR_565,
	ADE_XRGB_8888,
	ADE_XBGR_8888,
	ADE_ARGB_8888,
	ADE_ABGR_8888,
	ADE_RGBA_8888,
	ADE_BGRA_8888,
	ADE_RGB_888,
	ADE_BGR_888 = 9,
	ADE_FORMAT_NOT_SUPPORT = 800
};

/* ldi src cfg */
enum {
	TOP_DISP_SRC_NONE = 0,
	TOP_DISP_SRC_OVLY2,
	TOP_DISP_SRC_DISP,
	TOP_DISP_SRC_ROT,
	TOP_DISP_SRC_SCL2
};

enum {
	ADE_ISR_DMA_ERROR = 0x2000000
};

enum ade_channel {
	ADE_CH1 = 0,	/* channel 1 for primary plane */
	ADE_CH_NUM
};

enum ade_scale {
	ADE_SCL1 = 0,
	ADE_SCL2,
	ADE_SCL3,
	ADE_SCL_NUM
};

enum ade_ctran {
	ADE_CTRAN1 = 0,
	ADE_CTRAN2,
	ADE_CTRAN3,
	ADE_CTRAN4,
	ADE_CTRAN5,
	ADE_CTRAN6,
	ADE_CTRAN_NUM
};

enum ade_overlay {
	ADE_OVLY1 = 0,
	ADE_OVLY2,
	ADE_OVLY3,
	ADE_OVLY_NUM
};

enum {
	ADE_ALP_GLOBAL = 0,
	ADE_ALP_PIXEL,
	ADE_ALP_PIXEL_AND_GLB
};

enum {
	ADE_ALP_MUL_COEFF_0 = 0,	/* alpha */
	ADE_ALP_MUL_COEFF_1,		/* 1-alpha */
	ADE_ALP_MUL_COEFF_2,		/* 0 */
	ADE_ALP_MUL_COEFF_3		/* 1 */
};

/*
 * ADE Register Union Struct
 */
union U_ADE_CTRL1 {
struct {
	unsigned int	auto_clk_gate_en	:1;
	unsigned int	rot_buf_shr_out		:1;
	unsigned int	reserved_44		:30;
	} bits;
	unsigned int	u32;
};

union U_ADE_SOFT_RST_SEL0 {
struct {
	unsigned int    ch1_rdma_srst_sel     :1;
	unsigned int    ch2_rdma_srst_sel     :1;
	unsigned int    ch3_rdma_srst_sel     :1;
	unsigned int    ch4_rdma_srst_sel     :1;
	unsigned int    ch5_rdma_srst_sel     :1;
	unsigned int    ch6_rdma_srst_sel     :1;
	unsigned int    disp_rdma_srst_sel    :1;
	unsigned int    cmdq1_rdma_srst_sel   :1;
	unsigned int    cmdq2_rdma_srst_sel   :1;
	unsigned int    reserved_29           :1;
	unsigned int    ch1_wdma_srst_sel     :1;
	unsigned int    ch2_wdma_srst_sel     :1;
	unsigned int    ch3_wdma_srst_sel     :1;
	unsigned int    reserved_28           :1;
	unsigned int    cmdq_wdma_srst_sel    :1;
	unsigned int    clip1_srst_sel        :1;
	unsigned int    clip2_srst_sel        :1;
	unsigned int    clip3_srst_sel        :1;
	unsigned int    clip4_srst_sel        :1;
	unsigned int    clip5_srst_sel        :1;
	unsigned int    clip6_srst_sel        :1;
	unsigned int    scl1_srst_sel         :1;
	unsigned int    scl2_srst_sel         :1;
	unsigned int    scl3_srst_sel         :1;
	unsigned int    ctran1_srst_sel       :1;
	unsigned int    ctran2_srst_sel       :1;
	unsigned int    ctran3_srst_sel       :1;
	unsigned int    ctran4_srst_sel       :1;
	unsigned int    ctran5_srst_sel       :1;
	unsigned int    ctran6_srst_sel       :1;
	unsigned int    rot_srst_sel          :1;
	unsigned int    reserved_27           :1;
	} bits;
	unsigned int	u32;
};

union U_ADE_CTRL {
struct {
	unsigned int    frm_end_start         :2;
	unsigned int    dfs_buf_cfg           :1;
	unsigned int    rot_buf_cfg           :3;
	unsigned int    rd_ch5_nv             :1;
	unsigned int    rd_ch6_nv             :1;
	unsigned int    dfs_buf_unflow_lev1   :13;
	unsigned int    dfs_buf_unflow_lev2   :11;
	} bits;
	unsigned int	u32;
};

/*
 * ADE Register Write/Read functions
 */
static inline void set_TOP_CTL_clk_gate_en(u8 *base, u32 val)
{
	union U_ADE_CTRL1   ade_ctrl1;
	u8 *reg_addr = base + ADE_CTRL1;

	ade_ctrl1.u32 = readl(reg_addr);
	ade_ctrl1.bits.auto_clk_gate_en = val;
	writel(ade_ctrl1.u32, reg_addr);
}

static inline void set_TOP_SOFT_RST_SEL0_disp_rdma(u8 *base, u32 val)
{
	union U_ADE_SOFT_RST_SEL0 ade_soft_rst;
	u8 *addr = base + ADE_SOFT_RST_SEL0;

	ade_soft_rst.u32 = readl(addr);
	ade_soft_rst.bits.disp_rdma_srst_sel = val;
	writel(ade_soft_rst.u32, addr);
}

static inline void set_TOP_SOFT_RST_SEL0_ctran5(u8 *base, u32 val)
{
	union U_ADE_SOFT_RST_SEL0 ade_soft_rst;
	u8 *addr = base + ADE_SOFT_RST_SEL0;

	ade_soft_rst.u32 = readl(addr);
	ade_soft_rst.bits.ctran5_srst_sel = val;
	writel(ade_soft_rst.u32, addr);
}

static inline void set_TOP_SOFT_RST_SEL0_ctran6(u8 *base, u32 val)
{
	union U_ADE_SOFT_RST_SEL0 ade_soft_rst;
	u8 *addr = base + ADE_SOFT_RST_SEL0;

	ade_soft_rst.u32 = readl(addr);
	ade_soft_rst.bits.ctran6_srst_sel = val;
	writel(ade_soft_rst.u32, addr);
}

static inline void set_TOP_CTL_frm_end_start(u8 *base, u32 val)
{
	union U_ADE_CTRL  ade_ctrl;
	u8 *reg_addr = base + ADE_CTRL;

	ade_ctrl.u32 = readl(reg_addr);
	ade_ctrl.bits.frm_end_start = val;
	writel(ade_ctrl.u32, reg_addr);
}

/*
 * LDI Registers Offset
 */
#define LDI_HRZ_CTRL0		(0x7400)
#define LDI_HRZ_CTRL1		(0x7404)
#define LDI_VRT_CTRL0		(0x7408)
#define LDI_VRT_CTRL1		(0x740C)
#define LDI_PLR_CTRL		(0x7410)
#define LDI_DSP_SIZE		(0x7414)
#define LDI_INT_EN		(0x741C)
#define LDI_CTRL		(0x7420)
#define LDI_ORG_INT		(0x7424)
#define LDI_MSK_INT		(0x7428)
#define LDI_INT_CLR		(0x742C)
#define LDI_WORK_MODE		(0x7430)
#define LDI_DE_SPACE_LOW	(0x7438)
#define LDI_MCU_INTS		(0x7450)
#define LDI_MCU_INTE		(0x7454)
#define LDI_MCU_INTC		(0x7458)
#define LDI_HDMI_DSI_GT		(0x7434)

/*
 * LDI Timing Polarity defines
 */
#define HISI_LDI_FLAG_NVSYNC	BIT(0)
#define HISI_LDI_FLAG_NHSYNC	BIT(1)
#define HISI_LDI_FLAG_NPIXCLK	BIT(2)
#define HISI_LDI_FLAG_NDE	BIT(3)

/*
 * LDI Register Union Struct
 */
union U_LDI_CTRL {
struct {
	unsigned int    ldi_en                :1;
	unsigned int    disp_mode_buf         :1;
	unsigned int    date_gate_en          :1;
	unsigned int    bpp                   :2;
	unsigned int    wait_vsync_en         :1;
	unsigned int    corlorbar_width       :7;
	unsigned int    bgr                   :1;
	unsigned int    color_mode            :1;
	unsigned int    shutdown              :1;
	unsigned int    vactive_line          :12;
	unsigned int    ldi_en_self_clr       :1;
	unsigned int    reserved_573          :3;
	} bits;
	unsigned int    u32;
};

union U_LDI_WORK_MODE {
struct {
	unsigned int    work_mode             :1;
	unsigned int    wback_en              :1;
	unsigned int    colorbar_en           :1;
	unsigned int    reserved_577          :29;
	} bits;
	unsigned int    u32;
};

/*
 * LDI Register Write/Read Helper functions
 */
static inline void set_reg(u8 *addr, u32 val, u32 bw, u32 bs)
{
	u32 mask = (1 << bw) - 1;
	u32 tmp = readl(addr);

	tmp &= ~(mask << bs);
	writel(tmp | ((val & mask) << bs), addr);
}

static inline void set_LDI_CTRL_ldi_en(u8 *base, u32 val)
{
	union U_LDI_CTRL ldi_ctrl;
	u8 *addr = base + LDI_CTRL;

	ldi_ctrl.u32 = readl(addr);
	ldi_ctrl.bits.ldi_en = val;
	writel(ldi_ctrl.u32, addr);
}

static inline void set_LDI_CTRL_disp_mode(u8 *base, u32 val)
{
	union U_LDI_CTRL ldi_ctrl;
	u8 *addr = base + LDI_CTRL;

	ldi_ctrl.u32 = readl(addr);
	ldi_ctrl.bits.disp_mode_buf = val;
	writel(ldi_ctrl.u32, addr);
}

static inline void set_LDI_CTRL_bpp(u8 *base, u32 val)
{
	union U_LDI_CTRL ldi_ctrl;
	u8 *addr = base + LDI_CTRL;

	ldi_ctrl.u32 = readl(addr);
	ldi_ctrl.bits.bpp = val;
	writel(ldi_ctrl.u32, addr);
}

static inline void set_LDI_CTRL_corlorbar_width(u8 *base, u32 val)
{
	union U_LDI_CTRL ldi_ctrl;
	u8 *addr = base + LDI_CTRL;

	ldi_ctrl.u32 = readl(addr);
	ldi_ctrl.bits.corlorbar_width = (val > 0) ? val - 1 : 0;
	writel(ldi_ctrl.u32, addr);
}

static inline void set_LDI_CTRL_bgr(u8 *base, u32 val)
{
	union U_LDI_CTRL ldi_ctrl;
	u8 *addr = base + LDI_CTRL;

	ldi_ctrl.u32 = readl(addr);
	ldi_ctrl.bits.bgr = val;
	writel(ldi_ctrl.u32, addr);
}

static inline void set_LDI_WORK_MODE_work_mode(u8 *base, u32 val)
{
	union U_LDI_WORK_MODE ldi_work_mode;
	u8 *addr = base + LDI_WORK_MODE;

	ldi_work_mode.u32 = readl(addr);
	ldi_work_mode.bits.work_mode = val;
	writel(ldi_work_mode.u32, addr);
}

static inline void set_LDI_WORK_MODE_colorbar_en(u8 *base, u32 val)
{
	union U_LDI_WORK_MODE ldi_work_mode;
	u8 *addr = base + LDI_WORK_MODE;

	ldi_work_mode.u32 = readl(addr);
	ldi_work_mode.bits.colorbar_en = val;
	writel(ldi_work_mode.u32, addr);
}

#endif

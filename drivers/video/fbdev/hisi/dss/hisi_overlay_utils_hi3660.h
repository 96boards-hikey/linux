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
#ifndef _HISI_OVERLAY_UTILS_PLATFORM_H_
#define _HISI_OVERLAY_UTILS_PLATFORM_H_

#define HISI_DSS_VERSION_V400

#define GPIO_LCD_POWER_1V2  (54)
#define GPIO_LCD_STANDBY    (67)
#define GPIO_LCD_RESETN     (65)
#define GPIO_LCD_GATING     (60)
#define GPIO_LCD_PCLK_GATING (58)
#define GPIO_LCD_REFCLK_GATING (59)
#define GPIO_LCD_SPICS         (168)
#define GPIO_LCD_DRV_EN        (73)

#define GPIO_PG_SEL_A (72)
#define GPIO_TX_RX_A (74)
#define GPIO_PG_SEL_B (76)
#define GPIO_TX_RX_B (78)

/*******************************************************************************
 **
 */
#define CRGPERI_PLL0_CLK_RATE	(1600000000UL)
#define CRGPERI_PLL2_CLK_RATE	(960000000UL)
#define CRGPERI_PLL3_CLK_RATE	(1600000000UL)

#define DEFAULT_DSS_CORE_CLK_08V_RATE	(535000000UL)
#define DEFAULT_DSS_CORE_CLK_07V_RATE	(400000000UL)
#define DEFAULT_PCLK_DSS_RATE	(114000000UL)
#define DEFAULT_PCLK_PCTRL_RATE	(80000000UL)
#define DSS_MAX_PXL0_CLK_288M (288000000UL)

#define MMBUF_SIZE_MAX	(288 * 1024)
#define HISI_DSS_CMDLIST_MAX	(16)
#define HISI_DSS_CMDLIST_IDXS_MAX (0xFFFF)
#define HISI_DSS_COPYBIT_CMDLIST_IDXS	 (0xC000)
#define HISI_DSS_DPP_MAX_SUPPORT_BIT (0x7ff)
#define HISIFB_DSS_PLATFORM_TYPE  (FB_ACCEL_HI366x | FB_ACCEL_PLATFORM_TYPE_ASIC)

#define DSS_MIF_SMMU_SMRX_IDX_STEP (16)
#define CRG_PERI_DIS3_DEFAULT_VAL     (0x0002F000)
#define SCF_LINE_BUF	(2560)
#define DSS_GLB_MODULE_CLK_SEL_DEFAULT_VAL  (0xF0000008)
#define DSS_LDI_CLK_SEL_DEFAULT_VAL    (0x00000004)
#define DSS_DBUF_MEM_CTRL_DEFAULT_VAL  (0x00000008)
#define DSS_SMMU_RLD_EN0_DEFAULT_VAL    (0xffffffff)
#define DSS_SMMU_RLD_EN1_DEFAULT_VAL    (0xffffff8f)
#define DSS_SMMU_OUTSTANDING_VAL		(0xf)
#define DSS_MIF_CTRL2_INVAL_SEL3_STRIDE_MASK		(0xc)
#define DSS_AFBCE_ENC_OS_CFG_DEFAULT_VAL			(0x7)
#define TUI_SEC_RCH			(DSS_RCHN_V0)
#define DSS_CHN_MAX_DEFINE (DSS_COPYBIT_MAX)

/* perf stat */
#define DSS_DEVMEM_PERF_BASE						(0xFDF10000)
#define CRG_PERIPH_APB_PERRSTSTAT0_REG 				(0x68)
#define CRG_PERIPH_APB_IP_RST_PERF_STAT_BIT 		(18)
#define PERF_SAMPSTOP_REG 							(0x10)
#define DEVMEM_PERF_SIZE							(0x100)

#endif

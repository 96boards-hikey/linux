/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
 */

#ifndef UFS_HI3660_H_
#define UFS_HI3660_H_

#define HBRN8_POLL_TOUT_MS      1000

/*
 * pericrg specific define
 */
#define PEREN5_OFFSET		(0x050)
#define PERRSTEN3_OFFSET	(0x084)
#define PERRSTDIS3_OFFSET	(0x088)
#define PERRSTSTAT3_OFFSET	(0x08C)
#define CLKDIV16_OFFSET		(0x0E8)
#define CLKDIV17_OFFSET		(0x0EC)
#define CLKDIV21_OFFSET		(0x0FC)
#define UFS_ARESET		UFS_BIT(7)
#define RST_UFS			UFS_BIT(12)

/*
 * ufs sysctrl specific define
 */
#define PSW_POWER_CTRL			(0x04)
#define PHY_ISO_EN			(0x08)
#define HC_LP_CTRL			(0x0C)
#define PHY_CLK_CTRL			(0x10)
#define PSW_CLK_CTRL			(0x14)
#define CLOCK_GATE_BYPASS		(0x18)
#define RESET_CTRL_EN			(0x1C)
#define PHY_RESET_STATUS		(0x28)
#define UFS_SYSCTRL			(0x5C)
#define UFS_DEVICE_RESET_CTRL           (0x60)
#define UFS_APB_ADDR_MASK		(0x64)

#define BIT_UFS_PSW_ISO_CTRL		(1 << 16)
#define BIT_UFS_PSW_MTCMOS_EN		(1 << 0)
#define BIT_UFS_REFCLK_ISO_EN		(1 << 16)
#define BIT_UFS_PHY_ISO_CTRL		(1 << 0)
#define BIT_SYSCTRL_LP_ISOL_EN		(1 << 16)
#define BIT_SYSCTRL_LP_PWR_GATE		(1 << 0)
#define BIT_SYSCTRL_PWR_READY		(1 << 8)
#define BIT_SYSCTRL_REF_CLOCK_EN	(1 << 24)
#define MASK_SYSCTRL_REF_CLOCK_SEL	(0x3 << 8)
#define MASK_SYSCTRL_CFG_CLOCK_FREQ	(0xFF)
#define UFS_FREQ_CFG_CLK                (0x39)
#define BIT_SYSCTRL_PSW_CLK_EN		(1 << 4)
#define MASK_UFS_CLK_GATE_BYPASS	(0x3F)
#define BIT_STATUS_LP_RESETCOMPLETE	(1 << 0)
#define BIT_SYSCTRL_LP_RESET_N		(1 << 0)
#define BIT_UFS_REFCLK_SRC_SEl		(1 << 0)
#define MASK_UFS_SYSCRTL_BYPASS		(0x3F << 16)
#define MASK_UFS_DEVICE_RESET		(0x1 << 16)
#define BIT_UFS_DEVICE_RESET		(0x1)

/*
 * M-TX Configuration Attributes for hi3660
 */
#define MPHY_TX_FSM_STATE			0x41
#define TX_FSM_DISABLED				0x0
#define TX_FSM_HIBERN8				0x1
#define TX_FSM_SLEEP				0x2
#define TX_FSM_STALL				0x3
#define TX_FSM_LS_BURST				0x4
#define TX_FSM_HS_BURST				0x5
#define TX_FSM_LINE_CFG				0x6
#define TX_FSM_LINE_RESET			0x7

/*
 * hi3660 UFS HC specific Registers
 */
enum {
	UFS_REG_OCPTHRTL = 0xc0,
	UFS_REG_OOCPR    = 0xc4,

	UFS_REG_CDACFG   = 0xd0,
	UFS_REG_CDATX1   = 0xd4,
	UFS_REG_CDATX2   = 0xd8,
	UFS_REG_CDARX1   = 0xdc,
	UFS_REG_CDARX2   = 0xe0,
	UFS_REG_CDASTA   = 0xe4,

	UFS_REG_LBMCFG   = 0xf0,
	UFS_REG_LBMSTA   = 0xf4,
	UFS_REG_UFSMODE  = 0xf8,

	UFS_REG_HCLKDIV  = 0xfc,
};

#define UFS_AHIT_AUTOH8_TIMER           (0x1001)

/* REG UFS_REG_OCPTHRTL definition */
#define LP_PGE UFS_BIT(16)
#define LP_AH8_PGE UFS_BIT(17)

#define UFS_HCLKDIV_NORMAL_VALUE	0xE4
#define UFS_HCLKDIV_FPGA_VALUE		0x28

/* hi3660 UFS Unipro specific Registers */
#define VS_ULPH8_Cntrl 0xd0af
#define Ulp_Ulp_CtrlMode UFS_BIT(3)

/* vendor specific pre-defined parameters */
#define SLOW 1
#define FAST 2

#define UFS_HI3660_LIMIT_NUM_LANES_RX	2
#define UFS_HI3660_LIMIT_NUM_LANES_TX	2
#define UFS_HI3660_LIMIT_HSGEAR_RX	UFS_HS_G1
#define UFS_HI3660_LIMIT_HSGEAR_TX	UFS_HS_G1
#define UFS_HI3660_LIMIT_PWMGEAR_RX	UFS_PWM_G1
#define UFS_HI3660_LIMIT_PWMGEAR_TX	UFS_PWM_G1
#define UFS_HI3660_LIMIT_RX_PWR_PWM	SLOWAUTO_MODE
#define UFS_HI3660_LIMIT_TX_PWR_PWM	SLOWAUTO_MODE
#define UFS_HI3660_LIMIT_RX_PWR_HS	FASTAUTO_MODE
#define UFS_HI3660_LIMIT_TX_PWR_HS	FASTAUTO_MODE
#define UFS_HI3660_LIMIT_HS_RATE	PA_HS_MODE_A
#define UFS_HI3660_LIMIT_DESIRED_MODE	FAST

struct ufs_hi3660_host {
	struct ufs_hba *hba;
	void __iomem *ufs_sys_ctrl;
	struct reset_control	*rst;
	struct reset_control	*assert;
	uint64_t caps;
#define hi3660_CAP_RESERVED	UFS_BIT(0)
#define USE_SNPS_MPHY_TC	UFS_BIT(1)
#define USE_FPGA_BOARD_CLK	UFS_BIT(2)
#define USE_RATE_B		UFS_BIT(3)
#define BROKEN_FASTAUTO		UFS_BIT(4)
#define USE_ONE_LANE		UFS_BIT(5)
#define USE_HS_GEAR3		UFS_BIT(6)
#define USE_HS_GEAR2		UFS_BIT(7)
#define USE_HS_GEAR1		UFS_BIT(8)
#define USE_AUTO_H8		UFS_BIT(9)
#define BROKEN_CLK_GATE_BYPASS	UFS_BIT(10)

	int avail_ln_rx;
	int avail_ln_tx;

	u32 busthrtl_backup;
	u32 reset_gpio;

	bool in_suspend;

	struct ufs_pa_layer_attr dev_req_params;
};

#define ufs_sys_ctrl_writel(host, val, reg)                                    \
	writel((val), (host)->ufs_sys_ctrl + (reg))
#define ufs_sys_ctrl_readl(host, reg) readl((host)->ufs_sys_ctrl + (reg))
#define ufs_sys_ctrl_set_bits(host, mask, reg)                                 \
	ufs_sys_ctrl_writel(                                                   \
		(host), ((mask) | (ufs_sys_ctrl_readl((host), (reg)))), (reg))
#define ufs_sys_ctrl_clr_bits(host, mask, reg)                                 \
	ufs_sys_ctrl_writel((host),                                            \
			    ((~(mask)) & (ufs_sys_ctrl_readl((host), (reg)))), \
			    (reg))
#endif /* UFS_HI3660_H_ */

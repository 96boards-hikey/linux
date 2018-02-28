/* Copyright (c) 2017, HiSilicon. All rights reserved.
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

#ifndef UFS_HISI_H_
#define UFS_HISI_H_

#define HBRN8_POLL_TOUT_MS	1000

/*
 * SOC specific define
 */
#define SCDEEPSLEEPED_OFFSET	(0x008)
#define SCTRL_SCPERDIS4	(0x1B4)
#define SCTRL_SCCLKDIV9	(0x274)
#define SCTRL_SCPEREN4	(0x1B0)
#define CTRL_SCDEEPSLEEPED	(0x008)

#define BIT_UFS_SUBSYS_GT_CLK	UFS_BIT(14)
#define BIT_EFUSE_RHOLD	UFS_BIT(22)
#define MASK_UFS_MPHY_RHOLD	(0x1 << (11 + 16))
#define BIT_UFS_MPHY_RHOLD	UFS_BIT(11)

/*
 * pericrg specific define
 */
#define PERRSTEN3_OFFSET	(0x084)
#define PERRSTDIS3_OFFSET	(0x088)

#define UFS_ARESET	UFS_BIT(7)
#define RST_UFS	UFS_BIT(12)

/*
 * ufs sysctrl specific define
 */
#define PSW_POWER_CTRL	(0x04)
#define PHY_ISO_EN	(0x08)
#define HC_LP_CTRL	(0x0C)
#define PHY_CLK_CTRL	(0x10)
#define PSW_CLK_CTRL	(0x14)
#define CLOCK_GATE_BYPASS	(0x18)
#define RESET_CTRL_EN	(0x1C)
#define UFS_SYSCTRL	(0x5C)
#define UFS_DEVICE_RESET_CTRL	(0x60)
#define UFS_UMECTRL	(0x64)
#define CRG_UFS_CFG	(0x7C)

#define BIT_UFS_PSW_ISO_CTRL	UFS_BIT(16)
#define BIT_UFS_PSW_MTCMOS_EN	UFS_BIT(0)
#define BIT_UFS_REFCLK_ISO_EN	UFS_BIT(16)
#define BIT_UFS_PHY_ISO_CTRL	UFS_BIT(0)
#define BIT_SYSCTRL_LP_ISOL_EN	UFS_BIT(16)
#define BIT_SYSCTRL_PWR_READY	UFS_BIT(8)
#define BIT_SYSCTRL_REF_CLOCK_EN	UFS_BIT(24)
#define MASK_SYSCTRL_REF_CLOCK_SEL	(0x3 << 8)
#define MASK_SYSCTRL_CFG_CLOCK_FREQ	(0xFF)
#define UFS_FREQ_CFG_CLK	(0x39)
#define BIT_SYSCTRL_PSW_CLK_EN	UFS_BIT(4)
#define MASK_UFS_CLK_GATE_BYPASS	(0x3F)
#define BIT_SYSCTRL_LP_RESET_N	UFS_BIT(0)
#define BIT_UFS_REFCLK_SRC_SEl	UFS_BIT(0)
#define MASK_UFS_SYSCRTL_BYPASS	(0x3F << 16)
#define MASK_UFS_DEVICE_RESET	UFS_BIT(16)
#define BIT_UFS_DEVICE_RESET	UFS_BIT(0)
#define BIT_IES_EN_MASK	UFS_BIT(0)
#define BIT_CLK_DIV	UFS_BIT(16)

/*
 * M-TX Configuration Attributes for Hixxxx
 */
#define MPHY_TX_FSM_STATE	0x41
#define TX_FSM_HIBERN8	0x1

/*
 * Hixxxx UFS HC specific Registers
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

/* AHIT - Auto-Hibernate Idle Timer */
#define UFS_AHIT_AH8ITV_MASK	0x3FF

/* REG UFS_REG_OCPTHRTL definition */
#define UFS_HCLKDIV_NORMAL_VALUE	0xE4

/* vendor specific pre-defined parameters */
#define SLOW	1
#define FAST	2

#define UFS_HISI_LIMIT_NUM_LANES_RX	2
#define UFS_HISI_LIMIT_NUM_LANES_TX	2
#define UFS_HISI_LIMIT_HSGEAR_RX	UFS_HS_G3
#define UFS_HISI_LIMIT_HSGEAR_TX	UFS_HS_G3
#define UFS_HISI_LIMIT_PWMGEAR_RX	UFS_PWM_G4
#define UFS_HISI_LIMIT_PWMGEAR_TX	UFS_PWM_G4
#define UFS_HISI_LIMIT_RX_PWR_PWM	SLOW_MODE
#define UFS_HISI_LIMIT_TX_PWR_PWM	SLOW_MODE
#define UFS_HISI_LIMIT_RX_PWR_HS	FAST_MODE
#define UFS_HISI_LIMIT_TX_PWR_HS	FAST_MODE
#define UFS_HISI_LIMIT_HS_RATE	PA_HS_MODE_B
#define UFS_HISI_LIMIT_DESIRED_MODE	FAST

#define HISI_CAP_RESERVED	UFS_BIT(0)
#define HISI_CAP_UFS_PHY10nm	UFS_BIT(1)

struct ufs_hisi_host {
	struct ufs_hba *hba;
	void __iomem *ufs_sys_ctrl;
	void __iomem *sysctrl;

	struct reset_control	*rst;
	struct reset_control	*assert;

	uint64_t caps;

	bool in_suspend;
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

#define ufs_sctrl_writel(host, val, reg)                                    \
	writel((val), (host)->sysctrl + (reg))
#define ufs_sctrl_readl(host, reg) readl((host)->sysctrl + (reg))

#endif /* UFS_HISI_H_ */

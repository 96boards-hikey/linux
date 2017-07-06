/*
 * hisi_usb_vbus.h
 *
 * Copyright: (C) 2008-2018 hisilicon.
 * Contact: wangbinghui<wangbinghui@hisilicon.com>
 *
 * USB vbus for Hisilicon device
 *
 * This software is available to you under a choice of one of two
 * licenses. You may choose this file to be licensed under the terms
 * of the GNU General Public License (GPL) Version 2 or the 2-clause
 * BSD license listed below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 */
#ifndef _DWC3_HISI_H_
#define _DWC3_HISI_H_

#include <linux/pm_wakeup.h>
#include <linux/clk.h>
#include <linux/hisi/usb/hisi_usb.h>
#include <linux/regulator/consumer.h>

#define REG_BASE_PERI_CRG				(0xFFF35000)
#define PERI_CRG_CLK_EN4				(0x40)
#define PERI_CRG_CLK_DIS4				(0x44)
#define PERI_CRG_RSTDIS4				(0x94)
#define PERI_CRG_RSTEN4				(0x90)
#define PERI_CRG_ISODIS				(0x148)
#define PERI_CRG_ISOSTAT				(0x14C)
#define STCL_ADDR					(0xFFF0A214)
#ifndef BIT
#define BIT(x)	(1 << (x))
#endif
#define PERI_CRG_ISOSTAT_MODEMSUBSYSISOEN		 BIT(4)
#define PERI_CRG_ISODIS_MODEMSUBSYSISOEN		 BIT(4)

#define PCTRL_PERI_CTRL24				(0x64)
#define PCTRL_PERI_CTRL48				(0xC54)

#define IP_RST_USB3OTG_MUX				 BIT(8)
#define IP_RST_USB3OTG_AHBIF				 BIT(7)
#define IP_RST_USB3OTG_32K				 BIT(6)
#define IP_RST_USB3OTG					 BIT(5)
#define IP_RST_USB3OTGPHY_POR				 BIT(3)

#define GT_CLK_USB3OTG_REF				 BIT(0)
#define GT_ACLK_USB3OTG					 BIT(1)
#define GT_CLK_USB3PHY_REF				 BIT(2)

/*
 * hisi dwc3 phy registers
 */
#define DWC3_PHY_RX_OVRD_IN_HI	0x1006
#define DWC3_PHY_RX_SCOPE_VDCC	0x1026

/* DWC3_PHY_RX_SCOPE_VDCC */
#define RX_SCOPE_LFPS_EN	BIT(0)

/*
 * hisi dwc3 otg bc registers
 */
#define USBOTG3_CTRL0		0x00
#define USBOTG3_CTRL1		0x04
#define USBOTG3_CTRL2		0x08
#define USBOTG3_CTRL3		0x0C
#define USBOTG3_CTRL4		0x10
#define USBOTG3_CTRL5		0x14
#define USBOTG3_CTRL6		0x18
#define USBOTG3_CTRL7		0x1C
#define USBOTG3_STS0		0x20
#define USBOTG3_STS1		0x24
#define USBOTG3_STS2		0x28
#define USBOTG3_STS3		0x2C
#define BC_CTRL0		0x30
#define BC_CTRL1		0x34
#define BC_CTRL2		0x38
#define BC_STS0			0x3C
#define RAM_CTRL		0x40
#define USBOTG3_STS4		0x44
#define USB3PHY_CTRL		0x48
#define USB3PHY_STS		0x4C
#define USB3PHY_CR_STS		0x50
#define USB3PHY_CR_CTRL		0x54
#define USB3_RES		0x58

/* USTOTG3_CTRL0 */
# define USBOTG3CTRL0_SESSVLD_SEL              BIT(14)
# define USBOTG3CTRL0_SC_SESSVLD               BIT(13)
# define USBOTG3CTRL0_POWERPRESENT_SEL         BIT(12)
# define USBOTG3CTRL0_SC_POWERPRESENT          BIT(11)
# define USBOTG3CTRL0_BVALID_SEL               BIT(10)
# define USBOTG3CTRL0_SC_BVALID                BIT(9)
# define USBOTG3CTRL0_AVALID_SEL               BIT(8)
# define USBOTG3CTRL0_SC_AVALID                BIT(7)
# define USBOTG3CTRL0_VBUSVALID_SEL            BIT(6)
# define USBOTG3CTRL0_DRVVBUS                  BIT(5)
# define USBOTG3CTRL0_DRVVBUS_SEL              BIT(4)
# define USBOTG3CTRL0_IDDIG                    BIT(3)
# define USBOTG3CTRL0_IDDIG_SEL                BIT(2)
# define USBOTG3CTRL0_IDPULLUP                 BIT(1)
# define USBOTG3CTRL0_IDPULLUP_SEL             BIT(0)

/* USTOTG3_CTRL2 */
# define USBOTG3CTRL2_POWERDOWN_HSP             BIT(0)
# define USBOTG3CTRL2_POWERDOWN_SSP             BIT(1)

/* USBOTG3_CTRL3 */
# define USBOTG3_CTRL3_VBUSVLDEXT	BIT(6)
# define USBOTG3_CTRL3_VBUSVLDEXTSEL	BIT(5)
# define USBOTG3_CTRL3_TXBITSTUFFEHN	BIT(4)
# define USBOTG3_CTRL3_TXBITSTUFFEN	BIT(3)
# define USBOTG3_CTRL3_RETENABLEN	BIT(2)
# define USBOTG3_CTRL3_OTGDISABLE	BIT(1)
# define USBOTG3_CTRL3_COMMONONN	BIT(0)

/* USBOTG3_CTRL4 */
# define USBOTG3_CTRL4_TXVREFTUNE(x)            (((x) << 22) & (0xf << 22))
# define USBOTG3_CTRL4_TXRISETUNE(x)            (((x) << 20) & (3 << 20))
# define USBOTG3_CTRL4_TXRESTUNE(x)             (((x) << 18) & (3 << 18))
# define USBOTG3_CTRL4_TXPREEMPPULSETUNE        BIT(17)
# define USBOTG3_CTRL4_TXPREEMPAMPTUNE(x)       (((x) << 15) & (3 << 15))
# define USBOTG3_CTRL4_TXHSXVTUNE(x)            (((x) << 13) & (3 << 13))
# define USBOTG3_CTRL4_TXFSLSTUNE(x)            (((x) << 9) & (0xf << 9))
# define USBOTG3_CTRL4_SQRXTUNE(x)              (((x) << 6) & (7 << 6))
# define USBOTG3_CTRL4_OTGTUNE_MASK             (7 << 3)
# define USBOTG3_CTRL4_OTGTUNE(x)               \
(((x) << 3) & USBOTG3_CTRL4_OTGTUNE_MASK)
# define USBOTG3_CTRL4_COMPDISTUNE_MASK         7
# define USBOTG3_CTRL4_COMPDISTUNE(x)           \
((x) & USBOTG3_CTRL4_COMPDISTUNE_MASK)

# define USBOTG3_CTRL7_REF_SSP_EN				BIT(16)

/* USBOTG3_CTRL6 */
#define TX_VBOOST_LVL_MASK			7
#define TX_VBOOST_LVL(x)			((x) & TX_VBOOST_LVL_MASK)

/* BC_CTRL0 */
# define BC_CTRL0_BC_IDPULLUP		BIT(10)
# define BC_CTRL0_BC_SUSPEND_N		BIT(9)
# define BC_CTRL0_BC_DMPULLDOWN		BIT(8)
# define BC_CTRL0_BC_DPPULLDOWN		BIT(7)
# define BC_CTRL0_BC_TXVALIDH		BIT(6)
# define BC_CTRL0_BC_TXVALID		BIT(5)
# define BC_CTRL0_BC_TERMSELECT		BIT(4)
# define BC_CTRL0_BC_XCVRSELECT(x)	(((x) << 2) & (3 << 2))
# define BC_CTRL0_BC_OPMODE(x)		((x) & 3)

/* BC_CTRL1 */
# define BC_CTRL1_BC_MODE	1

/* BC_CTRL2 */
# define BC_CTRL2_BC_PHY_VDATDETENB	BIT(4)
# define BC_CTRL2_BC_PHY_VDATARCENB	BIT(3)
# define BC_CTRL2_BC_PHY_CHRGSEL		BIT(2)
# define BC_CTRL2_BC_PHY_DCDENB		BIT(1)
# define BC_CTRL2_BC_PHY_ACAENB		BIT(0)

/* BC_STS0 */
# define BC_STS0_BC_LINESTATE(x)	(((x) << 9) & (3 << 9))
# define BC_STS0_BC_PHY_CHGDET		BIT(8)
# define BC_STS0_BC_PHY_FSVMINUS	BIT(7)
# define BC_STS0_BC_PHY_FSVPLUS		BIT(6)
# define BC_STS0_BC_RID_GND		BIT(5)
# define BC_STS0_BC_RID_FLOAT		BIT(4)
# define BC_STS0_BC_RID_C		BIT(3)
# define BC_STS0_BC_RID_B		BIT(2)
# define BC_STS0_BC_RID_A		BIT(1)
# define BC_STS0_BC_SESSVLD		BIT(0)

/* USB3PHY_CR_STS */
#define USB3OTG_PHY_CR_DATA_OUT(x)	(((x) >> 1) & 0xffff)
#define USB3OTG_PHY_CR_ACK		BIT(0)

/* USB3PHY_CR_CTRL */
#define USB3OTG_PHY_CR_DATA_IN(x)	(((x) << 4) & (0xffff << 4))
#define USB3OTG_PHY_CR_WRITE		BIT(3)
#define USB3OTG_PHY_CR_READ		BIT(2)
#define USB3OTG_PHY_CR_CAP_DATA		BIT(1)
#define USB3OTG_PHY_CR_CAP_ADDR		BIT(0)

#define usb_dbg(format, arg...)    \
		pr_err("[USB3][%s]"format, __func__, ##arg)

#define usb_err(format, arg...)    \
		pr_err("[USB3][%s]"format, __func__, ##arg)

enum hisi_usb_state {
	USB_STATE_UNKNOWN = 0,
	USB_STATE_OFF,
	USB_STATE_DEVICE,
	USB_STATE_HOST,
};

struct hiusb_event_queue {
	enum otg_dev_event_type *event;
	unsigned int num_event;
	unsigned int max_event;
	unsigned int enpos, depos;
	unsigned int overlay, overlay_index;
};

#define MAX_EVENT_COUNT 16
#define EVENT_QUEUE_UNIT MAX_EVENT_COUNT

struct hisi_dwc3_device {
	struct platform_device *pdev;

	void __iomem *otg_bc_reg_base;
	void __iomem *pericfg_reg_base;
	void __iomem *pctrl_reg_base;
	void __iomem *sctrl_reg_base;

	struct regulator *usb_regu;
	unsigned int is_regu_on;
	unsigned int runtime_suspended;

	enum hisi_usb_state state;
	enum hisi_charger_type charger_type;
	enum hisi_charger_type fake_charger_type;

	enum otg_dev_event_type event;
	spinlock_t event_lock;

	struct mutex lock;
	struct wakeup_source ws;
	struct atomic_notifier_head charger_type_notifier;
	struct work_struct event_work;

	u32 eye_diagram_param;	/* this param will be set to USBOTG3_CTRL4 */
	u32 eye_diagram_host_param;
	u32 usb3_phy_cr_param;
	u32 usb3_phy_host_cr_param;
	u32 usb3_phy_tx_vboost_lvl;
	unsigned int host_flag;

	u32 fpga_flag;
	int fpga_usb_mode_gpio;

	struct clk *clk;
	struct clk *gt_aclk_usb3otg;

	int eventmask;

	/* for bc again */
	u32 bc_again_flag;
	struct delayed_work bc_again_work;
	struct notifier_block conndone_nb;

	/* event queue for handle event */
	struct hiusb_event_queue event_queue;

	struct usb3_phy_ops *phy_ops;

	unsigned int need_disable_vdp;
	void (*disable_vdp_src)(struct hisi_dwc3_device *hisi_dwc3);
};

#ifdef CONFIG_PM
extern const struct dev_pm_ops hisi_dwc3_dev_pm_ops;
#define HISI_DWC3_PM_OPS (&hisi_dwc3_dev_pm_ops)
#else
#define HISI_DWC3_PM_OPS NULL
#endif

struct usb3_phy_ops {
	struct regulator *subsys_regu;

	int (*init)(struct hisi_dwc3_device *hisi_dwc3);
	int (*shutdown)(struct hisi_dwc3_device *hisi_dwc3);
};

typedef ssize_t (*hiusb_debug_show_ops)(void *, char *, ssize_t);
typedef ssize_t (*hiusb_debug_store_ops)(void *, const char *, ssize_t);
void hiusb_debug_init(void *data);
void hiusb_debug_quick_register(void *dev_data,
				hiusb_debug_show_ops show,
				hiusb_debug_store_ops store);

void set_hisi_dwc3_power_flag(int val);
void config_femtophy_param(struct hisi_dwc3_device *hisi_dwc);
int hisi_dwc3_probe(struct platform_device *pdev, struct usb3_phy_ops *phy_ops);
int hisi_dwc3_remove(struct platform_device *pdev);
#endif /* _DWC3_HISI_H_ */

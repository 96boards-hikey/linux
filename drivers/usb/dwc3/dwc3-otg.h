/*
 * dwc3-otg.h
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
#ifndef __DRIVERS_USB_DWC3_OTG_H
#define __DRIVERS_USB_DWC3_OTG_H

/* BC Registers */
#define DWC3_BCFG	0xcc30
#define DWC3_BCEVT	0xcc38
#define DWC3_BCEVTEN	0xcc3c
#ifndef BIT
#define BIT(x)	(1 << (x))
#endif
/*  OTG Configuration Register */
#define DWC3_OCFG_DISPRTPWRCUTOFF	BIT(5)
#define DWC3_OCFG_OTGHIBDISMASK		BIT(4)
#define DWC3_OCFG_OTGSFTRSTMSK		BIT(3)
#define DWC3_OCFG_HNPCAP		BIT(1)
#define DWC3_OCFG_SRPCAP		1

/*  OTG Control Register */
#define	DWC3_OCTL_OTG3_GOERR		BIT(7)
#define	DWC3_OCTL_PERIMODE		BIT(6)
#define	DWC3_OCTL_PRTPWRCTL		BIT(5)
#define	DWC3_OCTL_HNPREQ		BIT(4)
#define	DWC3_OCTL_SESREQ		BIT(3)
#define	DWC3_OCTL_TERMSELDLPULSE	BIT(2)
#define	DWC3_OCTL_DEVSETHNPEN		BIT(1)
#define	DWC3_OCTL_HSTSETHNPEN		BIT(0)

/*  OTG Events Register */
#define DWC3_OEVT_DEVICEMOD			BIT(31)
#define DWC3_OEVT_OTGXHCIRUNSTPSETEVNT		BIT(27)
#define DWC3_OEVT_OTGDEVRUNSTPSETEVNT		BIT(26)
#define DWC3_OEVT_OTGHIBENTRYEVNT		BIT(25)
#define DWC3_OEVT_OTGCONIDSTSCHNGEVNT		BIT(24)
#define DWC3_OEVT_HRRCONFNOTIFEVNT		BIT(23)
#define DWC3_OEVT_HRRINITNOTIFEVNT		BIT(22)
#define DWC3_OEVT_OTGADEVIDLEEVNT		BIT(21)
#define DWC3_OEVT_OTGADEVBHOSTENDEVNT		BIT(20)
#define DWC3_OEVT_OTGADEVHOSTEVNT		BIT(19)
#define DWC3_OEVT_OTGADEVHNPCHNGEVNT		BIT(18)
#define DWC3_OEVT_OTGADEVSRPDETEVNT		BIT(17)
#define DWC3_OEVT_OTGADEVSESSENDDETEVNT		BIT(16)
#define DWC3_OEVT_OTGBDEVBHOSTENDEVNT		BIT(11)
#define DWC3_OEVT_OTGBDEVHNPCHNGEVNT		BIT(10)
#define DWC3_OEVT_OTGBDEVSESSVLDDETEVNT		BIT(9)
#define DWC3_OEVT_OTGBDEVVBUSCHNGEVNT		BIT(8)

/*  OTG Status Register */
#define DWC3_OSTS_OTGSTATE_MSK          (0xf << 8)
#define DWC3_OSTS_PERIPHERALSTATE       BIT(4)
#define DWC3_OSTS_XHCIPRTPOWER          BIT(3)
#define DWC3_OSTS_BSESVLD               BIT(2)
#define DWC3_OSTS_ASESVLD               BIT(1)
#define DWC3_OSTS_CONIDSTS              BIT(0)

struct dwc3_otg {
	struct usb_otg otg;
	struct dwc3 *dwc;
	int otg_irq;
	struct delayed_work otg_work;

	atomic_t otg_evt_flag;
#define DWC3_OTG_EVT_ID_SET 1
#define DWC3_OTG_EVT_ID_CLEAR 2
#define DWC3_OTG_EVT_VBUS_SET 3
#define DWC3_OTG_EVT_VBUS_CLEAR 4

	struct mutex lock;
};

#ifdef CONFIG_USB_DWC3_OTG
extern struct dwc3_otg *dwc_otg_handler;
int dwc3_otg_init(struct dwc3 *dwc);
void dwc3_otg_exit(struct dwc3 *dwc);
int dwc3_otg_work(struct dwc3_otg *dwc_otg, int evt);
int dwc3_otg_resume(struct dwc3 *dwc);
int dwc3_otg_suspend(struct dwc3 *dwc);
int dwc3_otg_id_value(struct dwc3_otg *dwc_otg);
#else
#define dwc_otg_handler ((struct dwc3_otg *)NULL)
static inline int dwc3_otg_init(struct dwc3 *dwc)
{
	return 0;
}

static inline void dwc3_otg_exit(struct dwc3 *dwc)
{
}

static inline int dwc3_otg_work(struct dwc3_otg *dwc_otg, int evt)
{
	return 0;
}

static inline int dwc3_otg_resume(struct dwc3 *dwc)
{
	return 0;
}

static inline int dwc3_otg_suspend(struct dwc3 *dwc)
{
	return 0;
}

static inline int dwc3_otg_id_value(struct dwc3_otg *dwc_otg)
{
	return 0;
};
#endif

#endif /* __DRIVERS_USB_DWC3_OTG_H */

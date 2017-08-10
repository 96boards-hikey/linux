#ifndef __DRIVERS_USB_DWC3_OTG_H
#define __DRIVERS_USB_DWC3_OTG_H

/* BC Registers */
#define DWC3_BCFG	0xcc30
#define DWC3_BCEVT	0xcc38
#define DWC3_BCEVTEN	0xcc3c

/*  OTG Configuration Register */
#define DWC3_OCFG_DISPRTPWRCUTOFF	(1 << 5)
#define DWC3_OCFG_OTGHIBDISMASK		(1 << 4)
#define DWC3_OCFG_OTGSFTRSTMSK		(1 << 3)
#define DWC3_OCFG_HNPCAP		(1 << 1)
#define DWC3_OCFG_SRPCAP		1

/*  OTG Control Register */
#define	DWC3_OCTL_OTG3_GOERR		(1 << 7)
#define	DWC3_OCTL_PERIMODE		(1 << 6)
#define	DWC3_OCTL_PRTPWRCTL		(1 << 5)
#define	DWC3_OCTL_HNPREQ		(1 << 4)
#define	DWC3_OCTL_SESREQ		(1 << 3)
#define	DWC3_OCTL_TERMSELDLPULSE	(1 << 2)
#define	DWC3_OCTL_DEVSETHNPEN		(1 << 1)
#define	DWC3_OCTL_HSTSETHNPEN		(1 << 0)

/*  OTG Events Register */
#define DWC3_OEVT_DEVICEMOD			(1 << 31)
#define DWC3_OEVT_OTGXHCIRUNSTPSETEVNT		(1 << 27)
#define DWC3_OEVT_OTGDEVRUNSTPSETEVNT		(1 << 26)
#define DWC3_OEVT_OTGHIBENTRYEVNT		(1 << 25)
#define DWC3_OEVT_OTGCONIDSTSCHNGEVNT		(1 << 24)
#define DWC3_OEVT_HRRCONFNOTIFEVNT		(1 << 23)
#define DWC3_OEVT_HRRINITNOTIFEVNT		(1 << 22)
#define DWC3_OEVT_OTGADEVIDLEEVNT		(1 << 21)
#define DWC3_OEVT_OTGADEVBHOSTENDEVNT		(1 << 20)
#define DWC3_OEVT_OTGADEVHOSTEVNT		(1 << 19)
#define DWC3_OEVT_OTGADEVHNPCHNGEVNT		(1 << 18)
#define DWC3_OEVT_OTGADEVSRPDETEVNT		(1 << 17)
#define DWC3_OEVT_OTGADEVSESSENDDETEVNT		(1 << 16)
#define DWC3_OEVT_OTGBDEVBHOSTENDEVNT		(1 << 11)
#define DWC3_OEVT_OTGBDEVHNPCHNGEVNT		(1 << 10)
#define DWC3_OEVT_OTGBDEVSESSVLDDETEVNT		(1 << 9)
#define DWC3_OEVT_OTGBDEVVBUSCHNGEVNT		(1 << 8)

/*  OTG Status Register */
#define DWC3_OSTS_OTGSTATE_MSK          (0xf << 8)
#define DWC3_OSTS_PERIPHERALSTATE       (1 << 4)
#define DWC3_OSTS_XHCIPRTPOWER          (1 << 3)
#define DWC3_OSTS_BSESVLD               (1 << 2)
#define DWC3_OSTS_ASESVLD               (1 << 1)
#define DWC3_OSTS_CONIDSTS              (1 << 0)

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
static inline void dwc3_otg_exit(struct dwc3 *dwc) {}
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

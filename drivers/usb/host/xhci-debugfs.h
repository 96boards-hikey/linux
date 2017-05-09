/*
 * =====================================================================================
 *
 *    Filename:  xhci-debugfs.h
 *
 *    Description:  Support for eyetest of host mode
 *
 *    Version:  1.0
 *    Created:  20160328
 *    Revision:  none
 *
 *    Author:  cwx211186
 *    Organization:  HISI_DRV
 *
 * =====================================================================================
 */

#ifdef CONFIG_DEBUG_FS
int xhci_create_debug_file(struct device *dev);
void xhci_remove_debug_file(void);
#else
static inline int xhci_create_debug_file(struct device *dev)
{
	return 0;
}

static inline void xhci_remove_debug_file(void) { }
#endif


/*
 * =====================================================================================
 *
 *    Filename:  xhci-debugfs.c
 *
 *    Description:  Support for eyetest of host mode
 *
 *    Version:  1.0
 *    Created:  20160223
 *    Revision:  none
 *
 *    Author:  cwx211186
 *    Organization:  HISI_DRV
 *
 * =====================================================================================
 */

#include <linux/device.h>
#include <linux/usb.h>
#include <linux/debugfs.h>

#include "xhci.h"

struct dentry *g_xhci_debug_file;

static ssize_t xhci_compliance_store(struct device *dev, const char *buf, size_t size)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	xhci_set_link_state(xhci, xhci->usb3_ports, 0, USB_SS_PORT_LS_COMP_MOD);
	dev_info(dev, "set link state compliance mode\n");

	return size;
}

static int xhci_compliance_show(struct seq_file *s, void *d)
{
	seq_printf(s, "usage: echo 1 > xhci_compliance\n");

	return 0;
}

static int xhci_compliance_open(struct inode *inode, struct file *file)
{
	return single_open(file, xhci_compliance_show, inode->i_private);
}

static ssize_t xhci_compliance_write(struct file *file, const char __user *buf,
		size_t size, loff_t *ppos)
{
	struct device *dev = ((struct seq_file *)file->private_data)->private;

	if (size > PAGE_SIZE) {
		printk(KERN_ERR "set charger type cmd too long!\n");
		return -ENOMEM;
	}

	xhci_compliance_store(dev, NULL, size);

	return size;
}
static const struct file_operations xhci_compliance_debug_fops = {
	.open = xhci_compliance_open,
	.read = seq_read,
	.write = xhci_compliance_write,
	.release = single_release,
};

int xhci_create_debug_file(struct device *dev)
{
#ifdef CONFIG_HISI_DEBUG_FS
	struct dentry *file;

	if (g_xhci_debug_file)
		return 0;

	file = debugfs_create_file("xhci_compliance", S_IWUSR | S_IRUSR,
			usb_debug_root, dev, &xhci_compliance_debug_fops);
	if (!file) {
		dev_err(dev, "create debugfs file error!\n");
		return -ENOMEM;
	}

	g_xhci_debug_file = file;
#endif

	return 0;
}

void xhci_remove_debug_file(void)
{
#ifdef CONFIG_HISI_DEBUG_FS
	if (!g_xhci_debug_file)
		return;

	debugfs_remove(g_xhci_debug_file);
	g_xhci_debug_file = NULL;
#endif
}

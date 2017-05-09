#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>

/* Types of reasons */
enum {
	NONE,
	BOOTLOADER,
	RECOVERY,
	OEM,
	MAX_REASONS
};

static u32			reasons[MAX_REASONS];
static void __iomem		*reboot_reason_val_addr;
static struct notifier_block	reboot_nb;

static int reboot_reason(struct notifier_block *nb, unsigned long action,
								void *data)
{
	char *cmd = (char *)data;
	u32 reason = reasons[NONE];

	if (!reboot_reason_val_addr)
		return NOTIFY_DONE;

	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10))
			reason = reasons[BOOTLOADER];
		else if (!strncmp(cmd, "recovery", 8))
			reason = reasons[RECOVERY];
		else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;

			if (!kstrtoul(cmd+4, 0, &code))
				reason = reasons[OEM] | (code & 0xff);
		}
	}

	if (reason != -1)
		writel(reason, reboot_reason_val_addr);
	return NOTIFY_DONE;
}

static int reboot_reason_probe(struct platform_device *pdev)
{
	struct resource *res;
	u32 val;
	int i;

	/* initialize the reasons */
	for (i = 0; i < MAX_REASONS; i++)
		reasons[i] = -1;

	/* Try to grab the reason io address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	reboot_reason_val_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(reboot_reason_val_addr))
		return PTR_ERR(reboot_reason_val_addr);

	/* initialize specified reasons from DT */
	if (!of_property_read_u32(pdev->dev.of_node, "reason,none", &val))
		reasons[NONE] = val;
	if (!of_property_read_u32(pdev->dev.of_node, "reason,bootloader", &val))
		reasons[BOOTLOADER] = val;
	if (!of_property_read_u32(pdev->dev.of_node, "reason,recovery", &val))
		reasons[RECOVERY] = val;
	if (!of_property_read_u32(pdev->dev.of_node, "reason,oem", &val))
		reasons[OEM] = val;

	/* Install the notifier */
	reboot_nb.notifier_call = reboot_reason;
	reboot_nb.priority = 256;
	if (register_reboot_notifier(&reboot_nb)) {
		dev_err(&pdev->dev,
			"failed to setup restart handler.\n");
	}
	return 0;
}

int reboot_reason_remove(struct platform_device *pdev)
{
	unregister_reboot_notifier(&reboot_nb);
	return 0;
}

static const struct of_device_id reboot_reason_of_match[] = {
	{ .compatible = "linux,reboot-reason-sram", },
	{ },
};
MODULE_DEVICE_TABLE(of, reboot_reason_of_match);

static struct platform_driver reboot_reason_driver = {
	.driver = {
		.name = "reboot-reason-sram",
		.of_match_table = reboot_reason_of_match,
	},
	.probe = reboot_reason_probe,
	.remove = reboot_reason_remove,
};
module_platform_driver(reboot_reason_driver);

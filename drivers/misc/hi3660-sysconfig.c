/*
 * For Hisilicon Hi3660 SoC, the reset of some hosts (e.g. UART) should be disabled
 * before using them, this driver will handle the host chip reset disable.
 *
 * Copyright (C) 2015 Hisilicon Ltd.
 * Author: Bintian Wang <bintian.wang@huawei.com>
 *
 */

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/printk.h>

static int __init hi3660_sysconf(void)
{
        static void __iomem *base = NULL;
	unsigned char data = 0;

	/* check BT I2S0, LEDs */
	base = ioremap(0xFFF11000, 0x0B0);
	if (!base) {
                pr_err("Unable to remap memory 0xFFF11000\n");
                return -ENOMEM;
        }

	pr_err("gpio179: 0x%x, WLAN_IRQ, should be 0\n", readl(base + 0x004));
	pr_err("gpio192: 0x%x, I2S0, BT_AUD_OUT, should be 1\n", readl(base + 0x034));
	pr_err("gpio193: 0x%x, I2S0, BT_AUD_IN,  should be 1\n", readl(base + 0x038));
	pr_err("gpio194: 0x%x, I2S0, BT_AUD_CLK, should be 1\n", readl(base + 0x03C));
	pr_err("gpio195: 0x%x, I2S0, BT_AUD_FSY, should be 1\n", readl(base + 0x040));

/*	pr_err("gpio205: 0x%x\n", readl(base + 0x064));
	pr_err("gpio207: 0x%x\n", readl(base + 0x06C));
	pr_err("gpio208: 0x%x\n", readl(base + 0x070));
	pr_err("gpio209: 0x%x\n", readl(base + 0x074));
	pr_err("gpio210: 0x%x\n", readl(base + 0x078));
	pr_err("gpio211: 0x%x\n", readl(base + 0x07C));
 */
        iounmap(base);

	/* check wifi_EN and bt_uart4*/
	base = ioremap(0xE896C000, 0x1F0);
	if (!base) {
                pr_err("Unable to remap memory 0xE896C000\n");
                return -ENOMEM;
        }

	pr_err("gpio051: 0x%x, WIFI_EN, should be 0\n", readl(base + 0x0C4));
	pr_err("gpio061: 0x%x, BT_UART4_CTS, should be 1\n", readl(base + 0x0EC));
	pr_err("gpio062: 0x%x, BT_UART4_RTS, should be 1\n", readl(base + 0x0F0));
	pr_err("gpio063: 0x%x, BT_UART4_RXD, should be 1\n", readl(base + 0x0F4));
	pr_err("gpio064: 0x%x, BT_UART4_TXD, should be 1\n", readl(base + 0x0F8));

        iounmap(base);

	/* check WiFi SDIO */
	base = ioremap(0xFF3FD000, 0x020);
	if (!base) {
                pr_err("Unable to remap memory 0xFF3FD000\n");
                return -ENOMEM;
        }

	pr_err("gpio168: 0x%x, WL_SDIO_CLK, should be 1\n", readl(base + 0x000));
	pr_err("gpio169: 0x%x, WL_SDIO_CMD, should be 1\n", readl(base + 0x004));
	pr_err("gpio170: 0x%x, WL_SDIO_D0, should be 1\n", readl(base + 0x008));
	pr_err("gpio171: 0x%x, WL_SDIO_D1, should be 1\n", readl(base + 0x00C));
	pr_err("gpio172: 0x%x, WL_SDIO_D2, should be 1\n", readl(base + 0x010));
	pr_err("gpio173: 0x%x, WL_SDIO_D3, should be 1\n", readl(base + 0x014));

        iounmap(base);


	/* adv7533 regulator power up */
	base = ioremap(0xfff34000, 0x1000);

	data = readb(base + (0x60 << 2)) | (1 << 1);
	writeb(data, base + (0x60 << 2));
	data = (readb(base + (0x61 << 2)) & ~(0xF)) | 2;
	writeb(data, base + (0x61 << 2));

	data = readb(base + (0x5C << 2)) | (1 << 1);
	writeb(data, base + (0x5C << 2));
	data = (readb(base + (0x5D << 2)) & ~(0xF)) | 9;
	writeb(data, base + (0x5D << 2));
	iounmap(base);

        return 0;
}
postcore_initcall(hi3660_sysconf);

/*
 * Copyright 2015, Guodong Xu <guodong.xu@linaro.org>
 * Copyright 2006, Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include "led.h"

#define BLUETOOTH_BLINK_DELAY 50 /* ms */

void bluetooth_led_rx(struct hci_dev *hdev)
{
	unsigned long led_delay = BLUETOOTH_BLINK_DELAY;
	if (unlikely(!hdev->rx_led))
		return;
	led_trigger_blink_oneshot(hdev->rx_led, &led_delay, &led_delay, 0);
}

void bluetooth_led_tx(struct hci_dev *hdev)
{
	unsigned long led_delay = BLUETOOTH_BLINK_DELAY;
	if (unlikely(!hdev->tx_led))
		return;
	led_trigger_blink_oneshot(hdev->tx_led, &led_delay, &led_delay, 0);
}

void bluetooth_led_names(struct hci_dev *hdev)
{
	snprintf(hdev->rx_led_name, sizeof(hdev->rx_led_name),
		 "%srx", hdev->name);
	snprintf(hdev->tx_led_name, sizeof(hdev->tx_led_name),
		 "%stx", hdev->name);
}

void bluetooth_led_init(struct hci_dev *hdev)
{
	hdev->rx_led = kzalloc(sizeof(struct led_trigger), GFP_KERNEL);
	if (hdev->rx_led) {
		hdev->rx_led->name = hdev->rx_led_name;
		if (led_trigger_register(hdev->rx_led)) {
			kfree(hdev->rx_led);
			hdev->rx_led = NULL;
		}
	}

	hdev->tx_led = kzalloc(sizeof(struct led_trigger), GFP_KERNEL);
	if (hdev->tx_led) {
		hdev->tx_led->name = hdev->tx_led_name;
		if (led_trigger_register(hdev->tx_led)) {
			kfree(hdev->tx_led);
			hdev->tx_led = NULL;
		}
	}
}

void bluetooth_led_exit(struct hci_dev *hdev)
{
	if (hdev->tx_led) {
		led_trigger_unregister(hdev->tx_led);
		kfree(hdev->tx_led);
	}
	if (hdev->rx_led) {
		led_trigger_unregister(hdev->rx_led);
		kfree(hdev->rx_led);
	}
}

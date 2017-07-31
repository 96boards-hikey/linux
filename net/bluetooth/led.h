/*
 * Copyright 2015, Guodong Xu <guodong.xu@linaro.org>
 * Copyright 2006, Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/leds.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#ifdef CONFIG_BT_LEDS
void bluetooth_led_rx(struct hci_dev *hdev);
void bluetooth_led_tx(struct hci_dev *hdev);
void bluetooth_led_names(struct hci_dev *hdev);
void bluetooth_led_init(struct hci_dev *hdev);
void bluetooth_led_exit(struct hci_dev *hdev);
#else
static inline void bluetooth_led_rx(struct hci_dev *hdev)
{
}
static inline void bluetooth_led_tx(struct hci_dev *hdev)
{
}
static inline void bluetooth_led_names(struct hci_dev *hdev)
{
}
static inline void bluetooth_led_init(struct hci_dev *hdev)
{
}
static inline void bluetooth_led_exit(struct hci_dev *hdev)
{
}
#endif

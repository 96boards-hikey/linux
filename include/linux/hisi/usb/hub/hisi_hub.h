/*
 * hub_usb5734.h
 *
 * Copyright (c) Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * Chenjun <chenjun@hisilicon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
void gpio_hub_power_on(void);
void gpio_hub_power_off(void);
void gpio_hub_switch_to_hub(void);
void gpio_hub_switch_to_typec(void);
void gpio_hub_typec_power_off(void);
void gpio_hub_typec_power_on(void);

/************************************************************
*
* Copyright (C), Hisilicon Tech. Co., Ltd.
* FileName: switch_fsa9685.h
* Author: Hisilicon       Version : 0.1      Date:  2013-11-07
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*  Description:    .h file for switch fsa9685
*  Version:
*  Function List:
*  History:
*  <author>  <time>   <version >   <desc>
***********************************************************/
#include <linux/hisi/usb/hisi_usb.h>

enum hisi_charger_type fsa9685_get_charger_type(void);
int fcp_read_switch_status (void);
int fcp_read_adapter_status(void);
void switch_dump_register(void);
void fsa9685_reg_dump(char* ptr);
int is_fcp_charger_type(void);

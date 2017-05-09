/************************************************************
*
* Copyright (C), Hisilicon Tech. Co., Ltd.
* FileName: switch_fsa9685.c
* Author: Hisilicon       Version : 0.1      Date:  2013-11-06
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
*  Description:    .c file for switch chip
*  Version:
*  Function List:
*  History:
*  <author>  <time>   <version >   <desc>
***********************************************************/

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/hisi/usb/switch/switch_usb.h>
#include "switch_chip.h"
#include <linux/hisi/usb/hisi_usb.h>
#include <linux/wakelock.h>
#include <linux/hisi/log/hisi_log.h>
#include <linux/hisi/usb/hisi_usb_rwswitch.h>
#include <linux/hisi/usb/switch/switch_fsa9685.h>
#include <linux/gpio/consumer.h>

struct mutex accp_detect_lock;
struct mutex accp_adaptor_reg_lock;

extern unsigned int get_boot_into_recovery_flag(void);
#define HISILOG_TAG switch_fsa9685
HISILOG_REGIST();

static int gpio = -1;
static struct i2c_client *this_client = NULL;
static struct work_struct   g_intb_work;
static struct delayed_work   detach_delayed_work;
static struct wake_lock usb_switch_lock;
#ifdef CONFIG_FSA9685_DEBUG_FS
static int reg_locked = 1;
static char chip_regs[0x5c+2] = { 0 };
#endif
static u32 fsa9685_usbid_enable = 1;
static u32 fsa9685_fcp_support = 0;
static u32 fsa9685_scp_support = 0;
static u32 fsa9685_mhl_detect_disable = 0;
static u32 two_switch_flag = 0;/*disable for two switch*/

static void usb_switch_wake_lock(void)
{
    if (!wake_lock_active(&usb_switch_lock)) {
        wake_lock(&usb_switch_lock);
        hisilog_info("usb switch wake lock\n");
    }
}
static void usb_switch_wake_unlock(void)
{
    if (wake_lock_active(&usb_switch_lock)) {
        wake_unlock(&usb_switch_lock);
        hisilog_info("usb switch wake unlock\n");
    }
}

static int fsa9685_write_reg(int reg, int val)
{
    int ret;
    if (NULL == this_client) {
        ret = -ERR_NO_DEV;
        hisilog_err("%s: this_client=NULL!!! ret=%d\n", __func__, ret);
        return ret;
    }
    ret = i2c_smbus_write_byte_data(this_client, reg, val);
    if (ret < 0)
        hisilog_info("%s: i2c write error!!! ret=%d\n", __func__, ret);

#ifdef CONFIG_FSA9685_DEBUG_FS
    chip_regs[reg] = val;
#endif
    return ret;
}

static int fsa9685_read_reg(int reg)
{
    int ret;
    if (NULL == this_client) {
        ret = -ERR_NO_DEV;
        hisilog_err("%s: this_client=NULL!!! ret=%d\n", __func__, ret);
        return ret;
    }
    ret = i2c_smbus_read_byte_data(this_client, reg);
    if (ret < 0)
        hisilog_info("%s: i2c read error!!! ret=%d\n", __func__, ret);

#ifdef CONFIG_FSA9685_DEBUG_FS
    chip_regs[reg] = ret;
#endif
    return ret;
}

static int fsa9685_write_reg_mask(int reg, int value,int mask)
{
    int val=0,ret=0;
    if (NULL == this_client) {
        ret = -ERR_NO_DEV;
        hisilog_err("%s: this_client=NULL!!! ret=%d\n", __func__, ret);
        return ret;
    }
    val= fsa9685_read_reg(reg);
    if(val < 0)
    {
        return val;
    }
    val &= ~mask;
    val |=value & mask;
    ret = fsa9685_write_reg(reg,val);
    return ret;
}

int fsa9685_manual_sw(int input_select)
{
    int value = 0, ret = 0;
    if (NULL == this_client) {
        ret = -ERR_NO_DEV;
        hisilog_err("%s: this_client=NULL!!! ret=%d\n", __func__, ret);
        return ret;
    }

    hisilog_info("%s: input_select = %d", __func__, input_select);
    /* Two switch not support USB2_ID*/
    if(two_switch_flag && (FSA9685_USB2_ID_TO_IDBYPASS == input_select))
    {
        return 0;
    }
    switch (input_select){
        case FSA9685_USB1_ID_TO_IDBYPASS:
            value = REG_VAL_FSA9685_USB1_ID_TO_IDBYPASS;
            break;
        case FSA9685_USB2_ID_TO_IDBYPASS:
            value = REG_VAL_FSA9685_USB2_ID_TO_IDBYPASS;
            break;
        case FSA9685_UART_ID_TO_IDBYPASS:
            value = REG_VAL_FSA9685_UART_ID_TO_IDBYPASS;
            break;
        case FSA9685_MHL_ID_TO_CBUS:
            value = REG_VAL_FSA9685_MHL_ID_TO_CBUS;
            break;
        case FSA9685_USB1_ID_TO_VBAT:
            value = REG_VAL_FSA9685_USB1_ID_TO_VBAT;
            break;
        case FSA9685_OPEN:
        default:
            value = REG_VAL_FSA9685_OPEN;
            break;
    }

    ret = fsa9685_write_reg(FSA9685_REG_MANUAL_SW_1, value);
    if ( ret < 0 ){
        ret = -ERR_FSA9685_REG_MANUAL_SW_1;
        hisilog_err("%s: write reg FSA9685_REG_MANUAL_SW_1 error!!! ret=%d\n", __func__, ret);
        return ret;
    }

    value = fsa9685_read_reg(FSA9685_REG_CONTROL);
    if (value < 0){
        ret = -ERR_FSA9685_READ_REG_CONTROL;
        hisilog_err("%s: read FSA9685_REG_CONTROL error!!! ret=%d\n", __func__, ret);
        return ret;
    }

    value &= (~FSA9685_MANUAL_SW); // 0: manual switching
    ret = fsa9685_write_reg(FSA9685_REG_CONTROL, value);
    if ( ret < 0 ){
        ret = -ERR_FSA9685_WRITE_REG_CONTROL;
        hisilog_err("%s: write FSA9685_REG_CONTROL error!!! ret=%d\n", __func__, ret);
        return ret;
    }

    return 0;

}
EXPORT_SYMBOL_GPL(fsa9685_manual_sw);

int fsa9685_manual_detach(void)
{
    int ret = 0;
    if (NULL == this_client){
        ret = -ERR_NO_DEV;
        hisilog_err("%s: this_client=NULL!!! ret=%d\n", __func__, ret);
        return ret;
    }

    schedule_delayed_work(&detach_delayed_work, msecs_to_jiffies(20));
    hisilog_info("%s: ------end.\n", __func__);
    return ret;
}
static void fsa9685_detach_work(struct work_struct *work)
{
    int ret;
    hisilog_info("%s: ------entry.\n", __func__);

    ret = fsa9685_read_reg(FSA9685_REG_DETACH_CONTROL);
    if ( ret < 0 ){
        hisilog_err("%s: read FSA9685_REG_DETACH_CONTROL error!!! ret=%d", __func__, ret);
        return;
    }

    ret = fsa9685_write_reg(FSA9685_REG_DETACH_CONTROL, 1);
    if ( ret < 0 ){
        hisilog_err("%s: write FSA9685_REG_DETACH_CONTROL error!!! ret=%d", __func__, ret);
        return;
    }

    hisilog_info("%s: ------end.\n", __func__);
    return;
}
EXPORT_SYMBOL_GPL(fsa9685_manual_detach);

int fsa9685_dcd_timeout_enable(bool enable_flag)
{
	int reg_val = 0;
	int ret;

	reg_val = fsa9685_read_reg(FSA9685_REG_DEVICE_ID);
	/*we need 9688c 9683 except 9688 not support enable dcd time out */
	if(FSA9688_VERSION_ID == ((reg_val & FAS9685_VERSION_ID_BIT_MASK) >> FAS9685_VERSION_ID_BIT_SHIFT)){
		return -1;
	}
	ret = fsa9685_write_reg_mask(FSA9685_REG_CONTROL2,enable_flag,FSA9685_DCD_TIME_OUT_MASK);
	if(ret < 0){
		hisilog_err("%s:write fsa9688c DCD enable_flag error!!!\n",__func__);
		return -1;
	}
	hisilog_info("%s:write fsa9688c DCD enable_flag is:%d!!!\n",__func__,enable_flag);
	return 0;
}

static void fsa9685_intb_work(struct work_struct *work);
static irqreturn_t fsa9685_irq_handler(int irq, void *dev_id)
{
    int gpio_value;

    usb_switch_wake_lock();
    gpio_value = gpio_get_value(gpio);
    if(gpio_value==1)
        hisilog_err("%s: intb high when interrupt occured!!!\n", __func__);

    schedule_work(&g_intb_work);

    hisilog_info("%s: ------end. gpio_value=%d\n", __func__, gpio_value);
    return IRQ_HANDLED;
}
static void fsa9685_intb_work(struct work_struct *work)
{
    int reg_ctl, reg_intrpt, reg_adc, reg_dev_type1, reg_dev_type2, reg_dev_type3, vbus_status;
    int ret = -1;
    int ret2 = 0;
    int id_valid_status = ID_VALID;
    int usb_switch_wakelock_flag = USB_SWITCH_NEED_WAKE_UNLOCK;
    static int invalid_times = 0;
    static int otg_attach = 0;
    static int pedestal_attach = 0;
    reg_intrpt = fsa9685_read_reg(FSA9685_REG_INTERRUPT);
    vbus_status = fsa9685_read_reg(FSA9685_REG_VBUS_STATUS);
    hisilog_info("%s: read FSA9685_REG_INTERRUPT. reg_intrpt=0x%x\n", __func__, reg_intrpt);
    /* if support fcp ,disable fcp interrupt */
    if(((0xFF != fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT_MASK1))
        ||(0xFF != fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT_MASK2))))
    {
        hisilog_info("disable fcp interrrupt again!!\n");
        ret2 |= fsa9685_write_reg_mask(FSA9685_REG_CONTROL2, FSA9685_ACCP_OSC_ENABLE,FSA9685_ACCP_OSC_ENABLE);
        ret2 |= fsa9685_write_reg_mask(FSA9685_REG_CONTROL2, 0,FSA9685_DCD_TIME_OUT_MASK);
        ret2 |= fsa9685_write_reg(FSA9685_REG_ACCP_INTERRUPT_MASK1, 0xFF);
        ret2 |= fsa9685_write_reg(FSA9685_REG_ACCP_INTERRUPT_MASK2, 0xFF);
        hisilog_info("%s : read ACCP interrupt,reg[0x59]=0x%x,reg[0x5A]=0x%x\n",__func__,
            fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT1), fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT2));
        if(ret2 < 0)
        {
            hisilog_err("accp interrupt mask write failed \n");
        }
    }

    if (unlikely(reg_intrpt < 0)) {
        hisilog_err("%s: read FSA9685_REG_INTERRUPT error!!!\n", __func__);
    } else if (unlikely(reg_intrpt == 0)) {
        hisilog_err("%s: read FSA9685_REG_INTERRUPT, and no intr!!!\n", __func__);
    } else {
        if (reg_intrpt & FSA9685_ATTACH){
            hisilog_info("%s: FSA9685_ATTACH\n", __func__);
            reg_dev_type1 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_1);
            reg_dev_type2 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_2);
            reg_dev_type3 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_3);
            hisilog_info("%s: reg_dev_type1=0x%X, reg_dev_type2=0x%X, reg_dev_type3= 0x%X\n", __func__,
                reg_dev_type1, reg_dev_type2, reg_dev_type3);
            if (reg_dev_type1 & FSA9685_FC_USB_DETECTED) {
                hisilog_info("%s: FSA9685_FC_USB_DETECTED\n", __func__);
            }
            if (reg_dev_type1 & FSA9685_USB_DETECTED){
                hisilog_info("%s: FSA9685_USB_DETECTED\n", __func__);
                if (FSA9685_USB2_ID_TO_IDBYPASS == get_swstate_value()){
                    switch_usb2_access_through_ap();
                    hisilog_info("%s: fsa9685 switch to USB2 by setvalue\n", __func__);
                }
            }
            if (reg_dev_type1 & FSA9685_UART_DETECTED) {
                hisilog_info("%s: FSA9685_UART_DETECTED\n", __func__);
            }
            if (reg_dev_type1 & FSA9685_MHL_DETECTED) {
                if (fsa9685_mhl_detect_disable == 1) {
                    hisilog_info("%s: mhl detection is not enabled on this platform, regard as an invalid detection\n", __func__);
                    id_valid_status = ID_INVALID;
                } else {
                    hisilog_info("%s: FSA9685_MHL_DETECTED\n", __func__);
                }
            }
            if (reg_dev_type1 & FSA9685_CDP_DETECTED) {
                hisilog_info("%s: FSA9685_CDP_DETECTED\n", __func__);
            }
            if (reg_dev_type1 & FSA9685_DCP_DETECTED) {
                hisilog_info("%s: FSA9685_DCP_DETECTED\n", __func__);
            }
            if ((reg_dev_type1 & FSA9685_USBOTG_DETECTED) && fsa9685_usbid_enable) {
                hisilog_info("%s: FSA9685_USBOTG_DETECTED\n", __func__);
                otg_attach = 1;
                usb_switch_wakelock_flag = USB_SWITCH_NEED_WAKE_LOCK;
                hisi_usb_id_change(ID_FALL_EVENT);
            }
            if (reg_dev_type1 & FSA9685_DEVICE_TYPE1_UNAVAILABLE) {
                id_valid_status = ID_INVALID;
                hisilog_info("%s: FSA9685_DEVICE_TYPE1_UNAVAILABLE_DETECTED\n", __func__);
            }
            if (reg_dev_type2 & FSA9685_JIG_UART) {
                hisilog_info("%s: FSA9685_JIG_UART\n", __func__);
            }
            if (reg_dev_type2 & FSA9685_DEVICE_TYPE2_UNAVAILABLE) {
                id_valid_status = ID_INVALID;
                hisilog_info("%s: FSA9685_DEVICE_TYPE2_UNAVAILABLE_DETECTED\n", __func__);
            }
            if (reg_dev_type3 & FSA9685_CUSTOMER_ACCESSORY7) {
                fsa9685_manual_sw(FSA9685_USB1_ID_TO_IDBYPASS);
                hisilog_info("%s:Enter FSA9685_CUSTOMER_ACCESSORY7\n", __func__);
            }
            if (reg_dev_type3 & FSA9685_CUSTOMER_ACCESSORY5) {
                hisilog_info("%s: FSA9685_CUSTOMER_ACCESSORY5, 365K\n", __func__);
                fsa9685_manual_sw(FSA9685_USB1_ID_TO_IDBYPASS);
                pedestal_attach = 1;
                usb_custom_acc5_event(pedestal_attach);
            }
            if (reg_dev_type3 & FSA9685_FM8_ACCESSORY) {
                hisilog_info("%s: FSA9685_FM8_DETECTED\n", __func__);
                fsa9685_manual_sw(FSA9685_USB1_ID_TO_IDBYPASS);
            }
            if (reg_dev_type3 & FSA9685_DEVICE_TYPE3_UNAVAILABLE) {
                id_valid_status = ID_INVALID;
                if (reg_intrpt & FSA9685_VBUS_CHANGE) {
                    fsa9685_manual_sw(FSA9685_USB1_ID_TO_IDBYPASS);
                }
                hisilog_info("%s: FSA9685_DEVICE_TYPE3_UNAVAILABLE_DETECTED\n", __func__);
            }
        }

        if (reg_intrpt & FSA9685_RESERVED_ATTACH) {
            id_valid_status = ID_INVALID;
            if (reg_intrpt & FSA9685_VBUS_CHANGE) {
                fsa9685_manual_sw(FSA9685_USB1_ID_TO_IDBYPASS);
            }
            hisilog_info("%s: FSA9685_RESERVED_ATTACH\n", __func__);
        }

        if (reg_intrpt & FSA9685_DETACH) {
            hisilog_info("%s: FSA9685_DETACH\n", __func__);
            /* check control register, if manual switch, reset to auto switch */
            reg_ctl = fsa9685_read_reg(FSA9685_REG_CONTROL);
            reg_dev_type2 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_2);
            hisilog_info("%s: reg_ctl=0x%x\n", __func__, reg_ctl);
            if (reg_ctl < 0){
                hisilog_err("%s: read FSA9685_REG_CONTROL error!!! reg_ctl=%d\n", __func__, reg_ctl);
                goto OUT;
            }
            if (0 == (reg_ctl & FSA9685_MANUAL_SW))
            {
                reg_ctl |= FSA9685_MANUAL_SW;
                ret = fsa9685_write_reg(FSA9685_REG_CONTROL, reg_ctl);
                if (ret < 0) {
                    hisilog_err("%s: write FSA9685_REG_CONTROL error!!!\n", __func__);
                    goto OUT;
                }
            }
            if ((otg_attach == 1) && fsa9685_usbid_enable) {
                hisilog_info("%s: FSA9685_USBOTG_DETACH\n", __func__);
                hisi_usb_id_change(ID_RISE_EVENT);
                otg_attach = 0;
            }
            if (pedestal_attach ==1) {
                hisilog_info("%s: FSA9685_CUSTOMER_ACCESSORY5_DETACH\n", __func__);
                pedestal_attach = 0;
                usb_custom_acc5_event(pedestal_attach);
            }
            if (reg_dev_type2 & FSA9685_JIG_UART) {
                hisilog_info("%s: FSA9685_JIG_UART\n", __func__);
            }
        }
        if (reg_intrpt & FSA9685_VBUS_CHANGE) {
            hisilog_info("%s: FSA9685_VBUS_CHANGE\n", __func__);
        }
        if (reg_intrpt & FSA9685_ADC_CHANGE) {
            reg_adc = fsa9685_read_reg(FSA9685_REG_ADC);
            hisilog_info("%s: FSA9685_ADC_CHANGE. reg_adc=%d\n", __func__, reg_adc);
            if (reg_adc < 0) {
                hisilog_err("%s: read FSA9685_ADC_CHANGE error!!! reg_adc=%d\n", __func__, reg_adc);
            }
            /* do user specific handle */
        }
    }

    if ((ID_INVALID == id_valid_status) &&
                (reg_intrpt & (FSA9685_ATTACH | FSA9685_RESERVED_ATTACH))) {
        invalid_times++;
        hisilog_info("%s: invalid time:%d reset fsa9685 work\n", __func__, invalid_times);

        if (invalid_times < MAX_DETECTION_TIMES) {
            hisilog_info("%s: start schedule delayed work\n", __func__);
            schedule_delayed_work(&detach_delayed_work, msecs_to_jiffies(0));
        } else {
            invalid_times = 0;
        }
    } else if ((ID_VALID == id_valid_status) &&
                (reg_intrpt & (FSA9685_ATTACH | FSA9685_RESERVED_ATTACH))) {
        invalid_times = 0;
    }

    if((USB_SWITCH_NEED_WAKE_UNLOCK == usb_switch_wakelock_flag) &&
            (0 == invalid_times)) {
        usb_switch_wake_unlock();
    }

OUT:
    hisilog_info("%s: ------end.\n", __func__);
    return;
}

#ifdef CONFIG_FSA9685_DEBUG_FS
static ssize_t dump_regs_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    const int regaddrs[] = {0x01, 0x02, 0x03, 0x04, 0x5, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d};
    const char str[] = "0123456789abcdef";
    int i = 0, index;
    char val = 0;
    for (i=0; i<0x60; i++) {
        if ((i%3)==2)
            buf[i]=' ';
        else
            buf[i] = 'x';
    }
    buf[0x5d] = '\n';
    buf[0x5e] = 0;
    buf[0x5f] = 0;
    if ( reg_locked != 0 ) {
        for (i=0; i<ARRAY_SIZE(regaddrs); i++) {
            if (regaddrs[i] != 0x03) {
                val = fsa9685_read_reg(regaddrs[i]);
                chip_regs[regaddrs[i]] = val;
            }
        }
    }

    for (i=0; i<ARRAY_SIZE(regaddrs); i++) {
        index = regaddrs[i];
        val = chip_regs[index];
            buf[3*index] = str[(val&0xf0)>>4];
        buf[3*index+1] = str[val&0x0f];
        buf[3*index+2] = ' ';
    }

    return 0x60;
}
static DEVICE_ATTR(dump_regs, S_IRUGO, dump_regs_show, NULL);
#endif

static ssize_t jigpin_ctrl_store(struct device *dev,
                          struct device_attribute *attr, const char *buf, size_t size)
{
    int jig_val = JIG_PULL_DEFAULT_DOWN;
    int ret = 0;
    if (NULL == this_client) {
        ret = -ERR_NO_DEV;
        hisilog_err("%s: this_client=NULL!!! ret=%d\n", __func__, ret);
        return ret;
    }
    if (sscanf(buf, "%d", &jig_val) != 1) {
        hisilog_err("%s:write regs failed, invalid input!\n", __func__);
        return -1;
    }
    ret = fsa9685_write_reg_mask(FSA9685_REG_CONTROL, 0, FSA9685_MANUAL_SW);
    if (ret < 0) {
        hisilog_err("%s:write FSA9685_REG_CONTROL error!\n",__func__);
        return ret;
    }
    if (FSA9683_I2C_ADDR == this_client->addr
          || CBTL9689_I2C_ADDR == this_client->addr) {
        ret = fsa9685_write_reg_mask(FSA9685_REG_WD_CTRL,
                   FSA9685_WD_CTRL_JIG_MANUAL_EN,FSA9685_WD_CTRL_JIG_MANUAL_EN);
        if (ret < 0) {
            hisilog_err("%s:write FSA9685_REG_WD_CTRL error!!!\n", __func__);
            return ret;
        }
    }
    switch (jig_val) {
        case JIG_PULL_DEFAULT_DOWN:
            hisilog_info("%s:pull down jig pin to default state\n", __func__);
            if (FSA9683_I2C_ADDR == this_client->addr) {
                ret = fsa9685_write_reg_mask(FSA9685_REG_MANUAL_SW_2,
                            FSA9683_REG_JIG_DEFAULT_DOWN, REG_JIG_MASK);
                if (ret < 0) {
                    hisilog_err("%s:write FSA9685_REG_MANUAL_SW_2 error!\n",__func__);
                    break;
                }
            } else {
                ret = fsa9685_write_reg_mask(FSA9685_REG_MANUAL_SW_2,
                            REG_JIG_DEFAULT_DOWN, REG_JIG_MASK);
                if (ret < 0) {
                    hisilog_err("%s:write FSA9685_REG_MANUAL_SW_2 error!\n",__func__);
                    break;
                }
            }
            break;
        case JIG_PULL_UP:
            hisilog_info("%s:pull up jig pin to cut battery\n", __func__);
            if(FSA9683_I2C_ADDR == this_client->addr){
                ret = fsa9685_write_reg_mask(FSA9685_REG_MANUAL_SW_2,
                            FSA9683_REG_JIG_UP, REG_JIG_MASK);
                if (ret < 0) {
                    hisilog_err("%s:write FSA9685_REG_MANUAL_SW_2 error!\n",__func__);
                }
            }else {
                ret = fsa9685_write_reg_mask(FSA9685_REG_MANUAL_SW_2,
                            REG_JIG_UP, REG_JIG_MASK);
                if (ret < 0) {
                    hisilog_err("%s:write FSA9685_REG_MANUAL_SW_2 error!\n",__func__);
                }
            }
            break;
        default:
            hisilog_err("%s:Wrong input action!\n", __func__);
            return -1;
    }
    return 0x60;
}

static ssize_t jigpin_ctrl_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    int manual_sw2_val;
    manual_sw2_val = fsa9685_read_reg(FSA9685_REG_MANUAL_SW_2);
    if (manual_sw2_val < 0) {
        hisilog_err("%s: read FSA9685_REG_MANUAL_SW_2 error!!! reg=%d.\n", __func__, manual_sw2_val);
    }

    return snprintf(buf, PAGE_SIZE, "%02x\n", manual_sw2_val);
}

static DEVICE_ATTR(jigpin_ctrl, S_IRUGO | S_IWUSR, jigpin_ctrl_show, jigpin_ctrl_store);

static ssize_t switchctrl_store(struct device *dev,
                          struct device_attribute *attr, const char *buf, size_t size)
{
    int action = 0;
    if (sscanf(buf, "%d", &action) != 1) {
        hisilog_err("%s:write regs failed, invalid input!\n", __func__);
        return -1;
    }
    switch (action) {
        case MANUAL_DETACH:
            hisilog_info("%s:manual_detach\n", __func__);
            fsa9685_manual_detach();
            break;
        case MANUAL_SWITCH:
            hisilog_info("%s:manual_switch\n", __func__);
            fsa9685_manual_sw(FSA9685_USB1_ID_TO_VBAT);
            break;
        default:
            hisilog_err("%s:Wrong input action!\n", __func__);
            return -1;
    }
    return 0x60;
}

static ssize_t switchctrl_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    int device_type1 = 0, device_type2 = 0, device_type3 = 0, mode = -1, tmp = 0;
    device_type1 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_1);
    if (device_type1 < 0) {
        hisilog_err("%s: read FSA9685_REG_DEVICE_TYPE_1 error!!! reg=%d.\n", __func__, device_type1);
        goto read_reg_failed;
    }
    device_type2 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_2);
    if (device_type2 < 0) {
        hisilog_err("%s: read FSA9685_REG_DEVICE_TYPE_2 error!!! reg=%d.\n", __func__, device_type2);
        goto read_reg_failed;
    }
    device_type3 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_3);
    if (device_type3 < 0) {
        hisilog_err("%s: read FSA9685_REG_DEVICE_TYPE_3 error!!! reg=%d.\n", __func__, device_type3);
        goto read_reg_failed;
    }
    hisilog_info("%s:device_type1=0x%x,device_type2=0x%x,device_type3=0x%x\n", __func__,device_type1,device_type2,device_type3);
    tmp = device_type3 << 16 | device_type2 << 8 | device_type1;
    mode = 0;
    while (tmp >> mode)
        mode++;
read_reg_failed:
    return sprintf(buf, "%d\n", mode);
}

static DEVICE_ATTR(switchctrl, S_IRUGO | S_IWUSR, switchctrl_show, switchctrl_store);

/****************************************************************************
  Function:     fcp_cmd_transfer_check
  Description:  check cmd transfer success or fail
  Input:         NA
  Output:       NA
  Return:        0: success
                   -1: fail
***************************************************************************/
int fcp_cmd_transfer_check(void)
{
    int reg_val1 = 0,reg_val2 =0,i =0;
    /*read accp interrupt registers until value is not zero */
    do{
        msleep(10);
        reg_val1 = fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT1);
        reg_val2 = fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT2);
        i++;
    }while(i<5 && reg_val1== 0 && reg_val2 == 0);

    if(reg_val1== 0 && reg_val2 == 0)
    {
        hisilog_info("%s : read accp interrupt time out,total time is %d ms\n",__func__,i*10);
    }
    if(reg_val1 < 0 || reg_val2 < 0 )
    {
        hisilog_err("%s: read  error!!! reg_val1=%d,reg_val2=%d \n", __func__, reg_val1,reg_val2);
        return -1;
    }

    /*if something  changed print reg info */
    if(reg_val2 & (FAS9685_PROSTAT | FAS9685_DSDVCSTAT) )
    {
        hisilog_info("%s : ACCP state changed  ,reg[0x59]=0x%x,reg[0x5A]=0x%x\n",__func__,reg_val1,reg_val2);
    }

    /* judge if cmd transfer success */
    if((reg_val1 & FAS9685_ACK) &&(reg_val1 & FAS9685_CMDCPL)
        && !(reg_val1 & FAS9685_CRCPAR)
        && !(reg_val2 & (FAS9685_CRCRX | FAS9685_PARRX)))
    {
        return 0;
    }
    else
    {
        hisilog_err("%s : reg[0x59]=0x%x,reg[0x5A]=0x%x\n",__func__,reg_val1,reg_val2);
        return -1;
    }
}
/****************************************************************************
  Function:     fcp_protocol_restart
  Description:  disable accp protocol and enable again
  Input:         NA
  Output:       NA
  Return:        0: success
                   -1: fail
***************************************************************************/
void fcp_protocol_restart(void)
{
    int reg_val =0;
    int ret = 0;
    /* disable accp protocol */
    fsa9685_write_reg_mask(FSA9685_REG_ACCP_CNTL, 0,FAS9685_ACCP_CNTL_MASK);
    msleep(100);
    reg_val = fsa9685_read_reg(FSA9685_REG_ACCP_STATUS);
    if(FSA9688_ACCP_STATUS_SLAVE_GOOD == (reg_val & FSA9688_ACCP_STATUS_MASK))
    {
        hisilog_err("%s : disable accp enable bit failed ,accp status [0x40]=0x%x  \n",__func__,reg_val);
    }

    /* enable accp protocol */
    fsa9685_write_reg_mask(FSA9685_REG_ACCP_CNTL, FSA9685_ACCP_IS_ENABLE,FAS9685_ACCP_CNTL_MASK);
    msleep(100);
    reg_val = fsa9685_read_reg(FSA9685_REG_ACCP_STATUS);
    if(FSA9688_ACCP_STATUS_SLAVE_GOOD != (reg_val & FSA9688_ACCP_STATUS_MASK))
    {
        hisilog_err("%s : enable accp enable bit failed, accp status [0x40]=0x%x  \n",__func__,reg_val);
    }
    /* disable accp interrupt */
    ret |= fsa9685_write_reg_mask(FSA9685_REG_CONTROL2, FSA9685_ACCP_OSC_ENABLE,FSA9685_ACCP_OSC_ENABLE);
    ret |= fsa9685_write_reg(FSA9685_REG_ACCP_INTERRUPT_MASK1, 0xFF);
    ret |= fsa9685_write_reg(FSA9685_REG_ACCP_INTERRUPT_MASK2, 0xFF);
    if(ret < 0)
    {
        hisilog_err("accp interrupt mask write failed \n");
    }
    hisilog_info("%s :disable and enable accp protocol accp status  is 0x%x \n",__func__,reg_val);
}
/****************************************************************************
  Function:     accp_adapter_reg_read
  Description:  read adapter register
  Input:        reg:register's num
                val:the value of register
  Output:       NA
  Return:        0: success
                -1: fail
***************************************************************************/
int accp_adapter_reg_read(int* val, int reg)
{
    int reg_val1 = 0,reg_val2 =0;
    int i=0,ret =0;
    mutex_lock(&accp_adaptor_reg_lock);
    for(i=0;i< FCP_RETRY_MAX_TIMES;i++)
    {
        /*before send cmd, read and clear accp interrupt registers */
        reg_val1 = fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT1);
        reg_val2 = fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT2);

        ret |= fsa9685_write_reg(FSA9685_REG_ACCP_CMD, FCP_CMD_SBRRD);
        ret |= fsa9685_write_reg(FSA9685_REG_ACCP_ADDR, reg);
        ret |= fsa9685_write_reg_mask(FSA9685_REG_ACCP_CNTL, FSA9685_ACCP_IS_ENABLE | FAS9685_ACCP_SENDCMD,FAS9685_ACCP_CNTL_MASK);
        if(ret)
        {
            hisilog_err("%s: write error ret is %d \n",__func__,ret);
            mutex_unlock(&accp_adaptor_reg_lock);
            return -1;
        }

        /* check cmd transfer success or fail */
        if(0 ==fcp_cmd_transfer_check())
        {
            /* recived data from adapter */
            *val = fsa9685_read_reg(FSA9685_REG_ACCP_DATA);
            break;
        }

        /* if transfer failed, restart accp protocol */
        fcp_protocol_restart();
        hisilog_err("%s : adapter register read fail times=%d ,register=0x%x,data=0x%x,reg[0x59]=0x%x,reg[0x5A]=0x%x \n",__func__,i,reg,*val,reg_val1,reg_val2);
    }
    hisilog_debug("%s : adapter register retry times=%d ,register=0x%x,data=0x%x,reg[0x59]=0x%x,reg[0x5A]=0x%x \n",__func__,i,reg,*val,reg_val1,reg_val2);
    if(FCP_RETRY_MAX_TIMES == i)
    {
        hisilog_err("%s : ack error,retry %d times \n",__func__,i);
        ret = -1;
    }
    else
    {
        ret = 0;
    }
    mutex_unlock(&accp_adaptor_reg_lock);
    return ret;
}

/****************************************************************************
  Function:     accp_adapter_reg_write
  Description:  write value into the adapter register
  Input:        reg:register's num
                val:the value of register
  Output:       NA
  Return:        0: success
                -1: fail
***************************************************************************/
int accp_adapter_reg_write(int val, int reg)
{
    int reg_val1 = 0,reg_val2 =0;
    int i = 0,ret = 0;
    mutex_lock(&accp_adaptor_reg_lock);
    for(i=0;i< FCP_RETRY_MAX_TIMES;i++)
    {
        /*before send cmd, clear accp interrupt registers */
        reg_val1 = fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT1);
        reg_val2 = fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT2);

        ret |=fsa9685_write_reg(FSA9685_REG_ACCP_CMD, FCP_CMD_SBRWR);
        ret |=fsa9685_write_reg(FSA9685_REG_ACCP_ADDR, reg);
        ret |=fsa9685_write_reg(FSA9685_REG_ACCP_DATA, val);
        ret |=fsa9685_write_reg_mask(FSA9685_REG_ACCP_CNTL, FSA9685_ACCP_IS_ENABLE | FAS9685_ACCP_SENDCMD,FAS9685_ACCP_CNTL_MASK);
        if(ret < 0)
        {
            hisilog_err("%s: write error ret is %d \n",__func__,ret);
            mutex_unlock(&accp_adaptor_reg_lock);
            return -1;
        }

        /* check cmd transfer success or fail */
        if(0 ==fcp_cmd_transfer_check())
        {
            break;
        }

        /* if transfer failed, restart accp protocol */
        fcp_protocol_restart();
        hisilog_err("%s : adapter register write fail times=%d ,register=0x%x,data=0x%x,reg[0x59]=0x%x,reg[0x5A]=0x%x \n",__func__,i,reg,val,reg_val1,reg_val2);
    }
    hisilog_debug("%s : adapter register retry times=%d ,register=0x%x,data=0x%x,reg[0x59]=0x%x,reg[0x5A]=0x%x \n",__func__,i,reg,val,reg_val1,reg_val2);

    if(FCP_RETRY_MAX_TIMES == i)
    {
        hisilog_err("%s : ack error,retry %d times \n",__func__,i);
        ret = -1;
    }
    else
    {
        ret = 0;
    }
    mutex_unlock(&accp_adaptor_reg_lock);
    return ret;
}

/****************************************************************************
  Function:     acp_adapter_detect
  Description:  detect accp adapter
  Input:        NA
  Output:       NA
  Return:        0: success
                -1: other fail
                1:fcp adapter but detect fail
***************************************************************************/
int accp_adapter_detect(void)
{
    int reg_val1 = 0;
    int reg_val2 = 0;
    int i = 0;
    if(NULL == this_client)
    {
        return ACCP_ADAPTOR_DETECT_OTHER;
    }

    mutex_lock(&accp_detect_lock);
    /*check accp status*/
    reg_val1 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_4);
    reg_val2 = fsa9685_read_reg(FSA9685_REG_ACCP_STATUS);
    if((reg_val1 & FSA9685_ACCP_CHARGER_DET)
        && (FSA9688_ACCP_STATUS_SLAVE_GOOD == (reg_val2 & FSA9688_ACCP_STATUS_MASK )))
    {
        hisilog_info("accp adapter detect ok.\n");
        mutex_unlock(&accp_detect_lock);
        return ACCP_ADAPTOR_DETECT_SUCC;
    }

    /* enable accp */
    reg_val1 = fsa9685_read_reg(FSA9685_REG_CONTROL2);
    reg_val1 |= FSA9685_ACCP_ENABLE;
    fsa9685_write_reg(FSA9685_REG_CONTROL2, reg_val1);

    /*detect hisi acp charger*/
    for(i=0; i < ACCP_DETECT_MAX_COUT; i++)
    {
        reg_val1 = fsa9685_read_reg(FSA9685_REG_DEVICE_TYPE_4);
        reg_val2 = fsa9685_read_reg(FSA9685_REG_ACCP_STATUS);
        if((reg_val1 & FSA9685_ACCP_CHARGER_DET)
            && (FSA9688_ACCP_STATUS_SLAVE_GOOD == (reg_val2 & FSA9688_ACCP_STATUS_MASK )))
        {
            break;
        }
        msleep(ACCP_POLL_TIME);
    }
    /*clear accp interrupt*/
    hisilog_info("%s : read ACCP interrupt,reg[0x59]=0x%x,reg[0x5A]=0x%x\n",__func__,
        fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT1), fsa9685_read_reg(FSA9685_REG_ACCP_INTERRUPT2));
    if(ACCP_DETECT_MAX_COUT == i )
    {
        mutex_unlock(&accp_detect_lock);
        hisilog_info("not accp adapter ,reg[0x%x]=0x%x,reg[0x%x]=0x%x \n",FSA9685_REG_DEVICE_TYPE_4,reg_val1,FSA9685_REG_ACCP_STATUS,reg_val2);
        if(reg_val1 & FSA9685_ACCP_CHARGER_DET)
        {
            return ACCP_ADAPTOR_DETECT_FAIL;/*accp adapter but detect fail */
        }
        return ACCP_ADAPTOR_DETECT_OTHER;/*not fcp adapter*/

    }
    hisilog_info("accp adapter detect ok,take %d ms \n",i*ACCP_POLL_TIME);
    mutex_unlock(&accp_detect_lock);
    return ACCP_ADAPTOR_DETECT_SUCC;
}

/**********************************************************
*  Function:        fsa9685_reg_dump
*  Discription:     dump register for charger dsm
*  Parameters:    ptr
*  return value:   void
**********************************************************/
#define DUMP_REG_NUM 21
#define DUMP_STR_LENTH 32

struct fsa9885_reg_dump_type
{
    unsigned char reg_add[DUMP_REG_NUM];
    unsigned char reg_val[DUMP_REG_NUM];
};

void fsa9685_reg_dump(char* ptr)
{
	const unsigned char reg_dump[DUMP_REG_NUM] = {0x01, 0x02, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x40, 0x41, 0x44, 0x47, 0x48, 0x5b, 0x5c};
	int i = 0;
	int val = 0;
	char buf[DUMP_STR_LENTH] = {0};

	struct fsa9885_reg_dump_type fsa9885_reg_dump;

	for(i = 0;i < sizeof(reg_dump)/sizeof(unsigned char);i++)
	{
		val = fsa9685_read_reg(reg_dump[i]);
		if (val < 0) {
			hisilog_err("%s: fsa9685_read_reg error!!!", __func__);
			return ;
		}
		fsa9885_reg_dump.reg_val[i] = val;
		fsa9885_reg_dump.reg_add[i] = reg_dump[i];
		val = 0;
	}

	snprintf(buf,sizeof(buf),"\n");
	strncat(ptr,buf,strlen(buf));
	memset(buf, 0, DUMP_STR_LENTH);

	for(i = 0;i < DUMP_REG_NUM;i++)
	{
		snprintf(buf,sizeof(buf),"reg[0x%2x]=0x%2x\n",fsa9885_reg_dump.reg_add[i],fsa9885_reg_dump.reg_val[i]);
		strncat(ptr,buf,strlen(buf));
		memset(buf, 0, DUMP_STR_LENTH);
	}
}
/****************************************************************************
  Function:     fcp_get_adapter_output_vol
  Description:  get fcp output vol
  Input:        NA.
  Output:       fcp output vol(5V/9V/12V)
  Return:        0: success
                -1: fail
***************************************************************************/
int fcp_get_adapter_output_vol(int *vol)
{
    int num = 0;
    int output_vol = 0;
    int ret =0;
    if(NULL == this_client)
    {
        return -1;
    }

    /*get adapter vol list number,exclude 5V*/
    ret = accp_adapter_reg_read(&num, FCP_SLAVE_REG_DISCRETE_CAPABILITIES);
    /*currently,fcp only support three out vol config(5v/9v/12v)*/
    if (ret || num > 2 )
    {
        hisilog_err("%s: vout list support err, reg[0x21] = %d.\n", __func__, num);
        return -1;
    }

    /*get max out vol value*/
   ret = accp_adapter_reg_read(&output_vol, FCP_SLAVE_REG_DISCRETE_OUT_V(num));
    if(ret )
    {
        hisilog_err("%s: get max out vol value failed ,ouputvol=%d,num=%d.\n",__func__,output_vol,num);
        return -1;
    }
    *vol = output_vol;
    hisilog_info("%s: get adapter max out vol = %d,num= %d.\n", __func__, output_vol,num);
    return 0;
}

/****************************************************************************
  Function:     fcp_get_adapter_max_power
  Description:  get fcp adpter max power
  Input:        NA.
  Output:       NA
  Return:       MAX POWER(W)
***************************************************************************/
int fcp_get_adapter_max_power(int *max_power)
{
    int reg_val = 0;
    int ret =0;
    if(NULL == this_client)
    {
        return -1;
    }

    /*read max power*/
    ret = accp_adapter_reg_read(&reg_val, FCP_SLAVE_REG_MAX_PWR);
    if(ret != 0)
    {
        hisilog_err("%s: read max power failed \n",__func__);
        return -1;
    }

    hisilog_info("%s: max power reg[0x22] = %d.\n", __func__, reg_val);
    *max_power = (reg_val >> 1);
    return 0;
}


/**********************************************************
*  Function:       fcp_mmi_show
*  Discription:    file node for mmi testing fast charge protocol
*  Parameters:     NA
*  return value:   0:success
*                  1:fail
*                  2:not support
**********************************************************/
static ssize_t fcp_mmi_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    int result = FCP_TEST_FAIL;
    enum hisi_charger_type type = hisi_get_charger_type();


    /*to avoid the usb port cutoff when CTS test*/
    if ((type == CHARGER_TYPE_SDP) || (type == CHARGER_TYPE_CDP))
    {
        result = FCP_TEST_FAIL;
        hisilog_err("fcp detect fail 1,charge type is %d \n",type);
        return snprintf(buf,PAGE_SIZE,"%d\n",result);
    }

    hisilog_info("%s: fcp mmi result  %d\n",__func__,result);
    return snprintf(buf,PAGE_SIZE,"%d\n",result);
}

static DEVICE_ATTR(fcp_mmi, S_IRUGO, fcp_mmi_show, NULL);

#ifdef CONFIG_OF
static const struct of_device_id switch_fsa9685_ids[] = {
    { .compatible = "hisilicon,fairchild_fsa9685" },
    { .compatible = "hisilicon,fairchild_fsa9683"},
    { .compatible = "hisilicon,nxp_cbtl9689" },
};
MODULE_DEVICE_TABLE(of, switch_fsa9685_ids);
#endif

static int fsa9685_probe(
    struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0, reg_ctl, gpio_value, reg_vendor = -1;
    struct device_node *node = client->dev.of_node;
#ifdef CONFIG_FSA9685_DEBUG_FS
    struct class *switch_class = NULL;
    struct device * new_dev = NULL;
#endif

    hisilog_info("%s: ------entry.\n", __func__);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        hisilog_err("%s: i2c_check_functionality error!!!\n", __func__);
        ret = -ERR_NO_DEV;
        this_client = NULL;
        goto err_i2c_check_functionality;
    }
    if(this_client)
    {
        hisilog_info("%s:chip is already detected\n", __func__);
		ret = -ERR_NO_DEV;
        return ret;
    }
    else
    {
        this_client = client;
    }
    /* distingush the chip with different address */
    reg_vendor = fsa9685_read_reg(FSA9685_REG_DEVICE_ID);
    if ( reg_vendor < 0 ) {
        hisilog_err("%s: read FSA9685_REG_DEVICE_ID error!!! reg_vendor=%d.\n", __func__, reg_vendor);
        goto err_i2c_check_functionality;
    }

#ifdef CONFIG_FSA9685_DEBUG_FS
    ret = device_create_file(&client->dev, &dev_attr_dump_regs);
    if (ret < 0) {
        hisilog_err("%s: device_create_file error!!! ret=%d.\n", __func__, ret);
        ret = -ERR_SWITCH_USB_DEV_REGISTER;
        goto err_i2c_check_functionality;
    }
#endif
/*create a node for phone-off current drain test*/
    ret = device_create_file(&client->dev, &dev_attr_switchctrl);
    if (ret < 0) {
        hisilog_err("%s: device_create_file error!!! ret=%d.\n", __func__, ret);
        ret = -ERR_SWITCH_USB_DEV_REGISTER;
        goto err_get_named_gpio;
    }

    ret = device_create_file(&client->dev, &dev_attr_jigpin_ctrl);
    if (ret < 0) {
        hisilog_err("%s: device_create_file error!!! ret=%d.\n", __func__, ret);
        ret = -ERR_SWITCH_USB_DEV_REGISTER;
        goto err_create_jigpin_ctrl_failed;
    }

    mutex_init(&accp_detect_lock);
    mutex_init(&accp_adaptor_reg_lock);
    ret = device_create_file(&client->dev, &dev_attr_fcp_mmi);
    if (ret < 0) {
        hisilog_err("%s: device_create_file error!!! ret=%d.\n", __func__, ret);
        ret = -ERR_SWITCH_USB_DEV_REGISTER;
        goto err_create_fcp_mmi_failed;
    }

    switch_class = class_create(THIS_MODULE, "usb_switch");
    if (IS_ERR(switch_class)) {
        hisilog_err("%s:create switch class failed!\n", __func__);
        goto err_create_link_failed;
    }
    new_dev = device_create(switch_class, NULL, 0, NULL, "switch_ctrl");
    if (NULL == new_dev) {
        hisilog_err("%s:device create failed!\n", __func__);
        goto err_create_link_failed;
    }
    ret = sysfs_create_link(&new_dev->kobj, &client->dev.kobj, "manual_ctrl");
    if (ret < 0) {
        hisilog_err("%s:create link to switch failed!\n", __func__);
        goto err_create_link_failed;
    }
    of_property_read_u32(node, "usbid-enable", &fsa9685_usbid_enable);
    of_property_read_u32(node, "fcp_support", &fsa9685_fcp_support);
    of_property_read_u32(node, "scp_support", &fsa9685_scp_support);
    of_property_read_u32(node, "mhl_detect_disable", &fsa9685_mhl_detect_disable);
    of_property_read_u32(node, "two-switch-flag", &two_switch_flag);

/*create link end*/
    gpio = of_get_named_gpio(node, "fairchild_fsa9685,gpio-intb", 0);
    if (gpio < 0) {
        hisilog_err("%s: of_get_named_gpio error!!! ret=%d, gpio=%d.\n", __func__, ret, gpio);
        ret = -ERR_OF_GET_NAME_GPIO;
        goto err_get_named_gpio;
    }
    hisilog_err("%s: of_get_named_gpio , gpio=%d.\n", __func__, gpio);

    ret = gpio_request(gpio, "fsa9685_int");
    if (ret < 0) {
        hisilog_err("%s: gpio_request error!!! ret=%d. gpio=%d.\n", __func__, ret, gpio);
        ret = -ERR_GPIO_REQUEST;
        goto err_get_named_gpio;
    }

    ret = gpio_direction_input(gpio);
    if (ret < 0) {
        hisilog_err("%s: gpio_direction_input error!!! ret=%d. gpio=%d.\n", __func__, ret, gpio);
        ret = -ERR_GPIO_DIRECTION_INPUT;
        goto err_gpio_direction_input;
    }

    client->irq = gpio_to_irq(gpio);
    if (client->irq < 0) {
        hisilog_err("%s: gpio_to_irq error!!! ret=%d, gpio=%d, client->irq=%d.\n", __func__, ret, gpio, client->irq);
        ret = -ERR_GPIO_TO_IRQ;
        goto err_get_named_gpio;
    }

    wake_lock_init(&usb_switch_lock, WAKE_LOCK_SUSPEND, "usb_switch_wakelock");

    INIT_DELAYED_WORK(&detach_delayed_work, fsa9685_detach_work);
    ret = fsa9685_write_reg(FSA9685_REG_DETACH_CONTROL, 1);
    if ( ret < 0 ){
        hisilog_err("%s: write FSA9685_REG_DETACH_CONTROL error!!! ret=%d", __func__, ret);
    }

    ret |= fsa9685_write_reg_mask(FSA9685_REG_CONTROL2, 0,FSA9685_DCD_TIME_OUT_MASK);
    if ( ret < 0 ){
        hisilog_err("%s: write FSA9685_REG_CONTROL2 FSA9685_DCD_TIME_OUT_MASK error!!! ret=%d", __func__, ret);
    }
    /* interrupt register */
    INIT_WORK(&g_intb_work, fsa9685_intb_work);

    ret = request_irq(client->irq,
               fsa9685_irq_handler,
               IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING,
               "fsa9685_int", client);
    if (ret < 0) {
        hisilog_err("%s: request_irq error!!! ret=%d.\n", __func__, ret);
        ret = -ERR_REQUEST_THREADED_IRQ;
        goto err_gpio_direction_input;
    }
    /* clear INT MASK */
    reg_ctl = fsa9685_read_reg(FSA9685_REG_CONTROL);
    if ( reg_ctl < 0 ) {
        hisilog_err("%s: read FSA9685_REG_CONTROL error!!! reg_ctl=%d.\n", __func__, reg_ctl);
        goto err_gpio_direction_input;
    }
    hisilog_info("%s: read FSA9685_REG_CONTROL. reg_ctl=0x%x.\n", __func__, reg_ctl);

    reg_ctl &= (~FSA9685_INT_MASK);
    ret = fsa9685_write_reg(FSA9685_REG_CONTROL, reg_ctl);
    if ( ret < 0 ) {
        hisilog_err("%s: write FSA9685_REG_CONTROL error!!! reg_ctl=%d.\n", __func__, reg_ctl);
        goto err_gpio_direction_input;
    }
    hisilog_info("%s: write FSA9685_REG_CONTROL. reg_ctl=0x%x.\n", __func__, reg_ctl);

    ret = fsa9685_write_reg(FSA9685_REG_DCD, 0x0c);
    if ( ret < 0 ) {
        hisilog_err("%s: write FSA9685_REG_DCD error!!! reg_DCD=0x%x.\n", __func__, 0x08);
        goto err_gpio_direction_input;
    }
    hisilog_info("%s: write FSA9685_REG_DCD. reg_DCD=0x%x.\n", __func__, 0x0c);

    gpio_value = gpio_get_value(gpio);
    hisilog_info("%s: intb=%d after clear MASK.\n", __func__, gpio_value);

    if (gpio_value == 0) {
        schedule_work(&g_intb_work);
    }

    hisilog_info("%s: ------end. ret = %d.\n", __func__, ret);
    return ret;

err_gpio_direction_input:
    gpio_free(gpio);
err_create_link_failed:
    device_remove_file(&client->dev, &dev_attr_fcp_mmi);
err_create_fcp_mmi_failed:
    device_remove_file(&client->dev, &dev_attr_jigpin_ctrl);
err_create_jigpin_ctrl_failed:
    device_remove_file(&client->dev, &dev_attr_switchctrl);
err_get_named_gpio:
    device_remove_file(&client->dev, &dev_attr_dump_regs);
err_i2c_check_functionality:
    this_client = NULL;

    hisilog_err("%s: ------FAIL!!! end. ret = %d.\n", __func__, ret);
    return ret;
}

static int fsa9685_remove(struct i2c_client *client)
{
    device_remove_file(&client->dev, &dev_attr_dump_regs);
    device_remove_file(&client->dev, &dev_attr_switchctrl);
    device_remove_file(&client->dev, &dev_attr_jigpin_ctrl);
    free_irq(client->irq, client);
    gpio_free(gpio);
    return 0;
}

static const struct i2c_device_id fsa9685_i2c_id[] = {
    { "fsa9685", 0 },
    { }
};

static struct i2c_driver fsa9685_i2c_driver = {
    .driver = {
        .name = "fsa9685",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(switch_fsa9685_ids),
    },
    .probe    = fsa9685_probe,
    .remove   = fsa9685_remove,
    .id_table = fsa9685_i2c_id,
};

static __init int fsa9685_i2c_init(void)
{
    int ret = 0;

    hisilog_info("%s: ------entry.\n", __func__);

    ret = i2c_add_driver(&fsa9685_i2c_driver);
    if(ret)
        hisilog_err("%s: i2c_add_driver error!!!\n", __func__);

    hisilog_info("%s: ------end.\n", __func__);
    return ret;
}

static __exit void fsa9685_i2c_exit(void)
{
    i2c_del_driver(&fsa9685_i2c_driver);
}

module_init(fsa9685_i2c_init);
module_exit(fsa9685_i2c_exit);

MODULE_AUTHOR("wangbinghui<wangbinghui@hisilicon.com>");
MODULE_DESCRIPTION("I2C bus driver for FSA9685 USB Accesory Detection Switch");
MODULE_LICENSE("GPL v2");

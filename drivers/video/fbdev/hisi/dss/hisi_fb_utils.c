/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */

#include "hisi_fb.h"
#include "hisi_overlay_utils.h"
#if defined (CONFIG_HISI_PERIDVFS)
#include "peri_volt_poll.h"
#endif

#define MAX_BUF 60
void set_reg(char __iomem *addr, uint32_t val, uint8_t bw, uint8_t bs)
{
	uint32_t mask = (1UL << bw) - 1UL;
	uint32_t tmp = 0;

	tmp = inp32(addr);
	tmp &= ~(mask << bs);

	outp32(addr, tmp | ((val & mask) << bs));

	if (g_debug_set_reg_val) {
		HISI_FB_INFO("writel: [%p] = 0x%x\n", addr,
			     tmp | ((val & mask) << bs));
	}
}

uint32_t set_bits32(uint32_t old_val, uint32_t val, uint8_t bw, uint8_t bs)
{
	uint32_t mask = (1UL << bw) - 1UL;
	uint32_t tmp = 0;

	tmp = old_val;
	tmp &= ~(mask << bs);

	return (tmp | ((val & mask) << bs));
}

void hisifb_set_reg(struct hisi_fb_data_type *hisifd,
		    char __iomem *addr, uint32_t val, uint8_t bw, uint8_t bs)
{
	set_reg(addr, val, bw, bs);
}

bool is_dss_idle_enable(void)
{
	return ((g_enable_dss_idle == 1) ? true : false);
}

uint32_t get_panel_xres(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);

	return ((hisifd->resolution_rect.w >
		 0) ? hisifd->resolution_rect.w : hisifd->panel_info.xres);
}

uint32_t get_panel_yres(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);

	return ((hisifd->resolution_rect.h >
		 0) ? hisifd->resolution_rect.h : hisifd->panel_info.yres);
}

uint32_t hisifb_line_length(int index, uint32_t xres, int bpp)
{
	return ALIGN_UP(xres * bpp, DMA_STRIDE_ALIGN);
}

void hisifb_get_timestamp(struct timeval *tv)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}

uint32_t hisifb_timestamp_diff(struct timeval *lasttime,
			       struct timeval *curtime)
{
	uint32_t ret;
	ret = (curtime->tv_usec >= lasttime->tv_usec) ?
	    curtime->tv_usec - lasttime->tv_usec :
	    1000000 - (lasttime->tv_usec - curtime->tv_usec);

	return ret;
}

void hisifb_save_file(char *filename, char *buf, uint32_t buf_len)
{
	ssize_t write_len = 0;
	struct file *fd = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	BUG_ON(filename == NULL);
	BUG_ON(buf == NULL);

	fd = filp_open(filename, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fd)) {
		HISI_FB_ERR("filp_open returned:filename %s, error %ld\n",
			    filename, PTR_ERR(fd));
		return;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	write_len = vfs_write(fd, (char __user *)buf, buf_len, &pos);

	pos = 0;
	set_fs(old_fs);
	filp_close(fd, NULL);
}

int hisifb_ctrl_on(struct hisi_fb_data_type *hisifd)
{
	struct hisi_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(hisifd == NULL);
	pdata = dev_get_platdata(&hisifd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (pdata->on) {
		ret = pdata->on(hisifd->pdev);
	}

	hisifb_vsync_resume(hisifd);
	hisi_overlay_on(hisifd, false);

	if (hisifd->panel_info.esd_enable) {
		hrtimer_start(&hisifd->esd_ctrl.esd_hrtimer,
			      ktime_set(ESD_CHECK_TIME_PERIOD / 1000,
					(ESD_CHECK_TIME_PERIOD % 1000) *
					1000000), HRTIMER_MODE_REL);
	}

	return ret;
}

int hisifb_ctrl_off(struct hisi_fb_data_type *hisifd)
{
	struct hisi_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(hisifd == NULL);
	pdata = dev_get_platdata(&hisifd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (hisifd->panel_info.esd_enable) {
		hrtimer_cancel(&hisifd->esd_ctrl.esd_hrtimer);
	}

	hisifb_vsync_suspend(hisifd);
	hisi_overlay_off(hisifd);

	if (pdata->off) {
		ret = pdata->off(hisifd->pdev);
	}

	if ((hisifd->index == PRIMARY_PANEL_IDX) ||
	    (hisifd->index == EXTERNAL_PANEL_IDX)) {

		hisifb_layerbuf_unlock(hisifd,
				       &(hisifd->buf_sync_ctrl.layerbuf_list));
	}

	return ret;
}

int hisifb_ctrl_dss_clk_rate_set(struct fb_info *info, void __user *argp)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;
	dss_clk_rate_t dss_clk_rate;

	if (NULL == info) {
		HISI_FB_ERR("NULL Pointer!\n");
		return -EINVAL;
	}

	hisifd = (struct hisi_fb_data_type *)info->par;
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer!\n");
		return -EINVAL;
	}

	if (hisifd->index != PRIMARY_PANEL_IDX) {
		HISI_FB_ERR("fb%d, not supported!\n", hisifd->index);
		return -EINVAL;
	}

	if (NULL == argp) {
		HISI_FB_ERR("NULL Pointer!\n");
		return -EINVAL;
	}

	if (hisifd->core_clk_upt_support == 0) {
		HISI_FB_DEBUG("no support core_clk_upt\n");
		return ret;
	}

	ret = copy_from_user(&dss_clk_rate, argp, sizeof(dss_clk_rate_t));
	if (ret) {
		HISI_FB_ERR("copy_from_user failed!ret=%d.", ret);
		return ret;
	}

	down(&hisifd->blank_sem);

	if (!hisifd->panel_power_on) {
		HISI_FB_DEBUG("fb%d, panel power off!\n", hisifd->index);
		ret = -EPERM;
		goto err_out;
	}

	ret = set_dss_clk_rate(hisifd, dss_clk_rate);

 err_out:
	up(&hisifd->blank_sem);

	return ret;
}

/*lint +e665, +e514, +e84, +e886, +e846, +e778*/
void hisifb_sysfs_attrs_add(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	if (hisifd->sysfs_attrs_append_fnc) {
		/* hisifd->sysfs_attrs_append_fnc(hisifd, &dev_attr_lcd_model.attr); */
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);
}

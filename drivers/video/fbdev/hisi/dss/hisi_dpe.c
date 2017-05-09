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
#include "hisi_dpe_utils.h"
#include "hisi_overlay_utils.h"
#ifdef CONFIG_HISI_OCBC
#include <linux/hisi/ocbc.h>
#endif

static int dpe_init(struct hisi_fb_data_type *hisifd, bool fastboot_enable)
{
	BUG_ON(hisifd == NULL);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		init_post_scf(hisifd);
		init_dbuf(hisifd);
		init_dpp(hisifd);
		/* init_sbl(hisifd); */
		init_acm(hisifd);
		init_dpp_csc(hisifd);
		init_igm_gmp_xcc_gm(hisifd);

		init_ifbc(hisifd);
		init_ldi(hisifd, fastboot_enable);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		if (hisifd->dss_pxl1_clk)
			clk_disable(hisifd->dss_pxl1_clk);

		set_reg(hisifd->dss_base + DSS_LDI0_OFFSET + LDI_DSI1_CLK_SEL,
			0x1, 1, 0);

		if (hisifd->dss_pxl1_clk)
			clk_enable(hisifd->dss_pxl1_clk);

		set_reg(hisifd->dss_base + DSS_LDI0_OFFSET + LDI_DSI1_RST_SEL,
			0x1, 1, 0);
		/* dual lcd: dsi_mux_sel=1, dual mipi: dsi_mux_sel=0 */
		set_reg(hisifd->dss_base + DSS_MCTRL_SYS_OFFSET +
			MCTL_DSI_MUX_SEL, 0x1, 1, 0);

		init_dbuf(hisifd);
		init_ldi(hisifd, fastboot_enable);
	} else if (hisifd->index == AUXILIARY_PANEL_IDX) {
		;
	} else {
		HISI_FB_ERR("fb%d, not support this device!\n", hisifd->index);
	}

	return 0;
}

static int dpe_deinit(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		deinit_ldi(hisifd);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		deinit_ldi(hisifd);
	} else if (hisifd->index == AUXILIARY_PANEL_IDX) {
		;
	} else {
		HISI_FB_ERR("fb%d, not support this device!\n", hisifd->index);
	}

	return 0;
}

static void dpe_check_itf_status(struct hisi_fb_data_type *hisifd)
{
	int tmp = 0;
	int delay_count = 0;
	bool is_timeout = true;
	int itf_idx = 0;
	char __iomem *mctl_sys_base = NULL;

	BUG_ON(hisifd == NULL);

	if ((hisifd->index == PRIMARY_PANEL_IDX) ||
	    (hisifd->index == EXTERNAL_PANEL_IDX)) {
		itf_idx = hisifd->index;
		mctl_sys_base = hisifd->dss_base + DSS_MCTRL_SYS_OFFSET;

		while (1) {
			tmp =
			    inp32(mctl_sys_base + MCTL_MOD17_STATUS +
				  itf_idx * 0x4);
			if (((tmp & 0x10) == 0x10) || delay_count > 100) {
				is_timeout = (delay_count > 100) ? true : false;
				delay_count = 0;
				break;
			} else {
				mdelay(1);
				++delay_count;
			}
		}

		if (is_timeout) {
			HISI_FB_DEBUG
			    ("mctl_itf%d not in idle status,ints=0x%x !\n",
			     hisifd->index, tmp);
		}
	}
}

static int dpe_irq_enable(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);

	if (hisifd->dpe_irq)
		enable_irq(hisifd->dpe_irq);

	return 0;
}

static int dpe_irq_disable(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);

	if (hisifd->dpe_irq)
		disable_irq(hisifd->dpe_irq);

	return 0;
}

static int dpe_irq_disable_nosync(struct hisi_fb_data_type *hisifd)
{
	BUG_ON(hisifd == NULL);

	if (hisifd->dpe_irq)
		disable_irq_nosync(hisifd->dpe_irq);

	return 0;
}

int dpe_common_clk_enable(struct hisi_fb_data_type *hisifd)
{
	int ret = 0;
	struct clk *clk_tmp = NULL;

	BUG_ON(hisifd == NULL);

#ifdef CONFIG_DSS_MMBUF_CLK_USED
	clk_tmp = hisifd->dss_mmbuf_clk;
	if (clk_tmp) {
		ret = clk_prepare(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_mmbuf_clk clk_prepare failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}

		ret = clk_enable(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_mmbuf_clk clk_enable failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}
	}
#endif

	clk_tmp = hisifd->dss_axi_clk;
	if (clk_tmp) {
		ret = clk_prepare(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_axi_clk clk_prepare failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}

		ret = clk_enable(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_axi_clk clk_enable failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}
	}

	clk_tmp = hisifd->dss_pclk_dss_clk;
	if (clk_tmp) {
		ret = clk_prepare(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_pclk_dss_clk clk_prepare failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}

		ret = clk_enable(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_pclk_dss_clk clk_enable failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}
	}

	return 0;
}

int dpe_inner_clk_enable(struct hisi_fb_data_type *hisifd)
{
	int ret = 0;
	struct clk *clk_tmp = NULL;

	BUG_ON(hisifd == NULL);

	clk_tmp = hisifd->dss_pri_clk;
	if (clk_tmp) {
		ret = clk_prepare(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_pri_clk clk_prepare failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}

		ret = clk_enable(clk_tmp);
		if (ret) {
			HISI_FB_ERR
			    ("fb%d dss_pri_clk clk_enable failed, error=%d!\n",
			     hisifd->index, ret);
			return -EINVAL;
		}
	}

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		clk_tmp = hisifd->dss_pxl0_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pxl0_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pxl0_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		clk_tmp = hisifd->dss_pxl1_clk;
		if (clk_tmp) {
			ret = clk_prepare(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pxl1_clk clk_prepare failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}

			ret = clk_enable(clk_tmp);
			if (ret) {
				HISI_FB_ERR
				    ("fb%d dss_pxl1_clk clk_enable failed, error=%d!\n",
				     hisifd->index, ret);
				return -EINVAL;
			}
		}
	} else {
		;
	}

	return 0;
}

int dpe_common_clk_disable(struct hisi_fb_data_type *hisifd)
{
	struct clk *clk_tmp = NULL;

	BUG_ON(hisifd == NULL);

	clk_tmp = hisifd->dss_pclk_dss_clk;
	if (clk_tmp) {
		clk_disable(clk_tmp);
		clk_unprepare(clk_tmp);
	}

	clk_tmp = hisifd->dss_axi_clk;
	if (clk_tmp) {
		clk_disable(clk_tmp);
		clk_unprepare(clk_tmp);
	}
#ifdef CONFIG_DSS_MMBUF_CLK_USED
	clk_tmp = hisifd->dss_mmbuf_clk;
	if (clk_tmp) {
		clk_disable(clk_tmp);
		clk_unprepare(clk_tmp);
	}
#endif

	return 0;
}

int dpe_inner_clk_disable(struct hisi_fb_data_type *hisifd)
{
	struct clk *clk_tmp = NULL;

	BUG_ON(hisifd == NULL);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		clk_tmp = hisifd->dss_pxl0_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		clk_tmp = hisifd->dss_pxl1_clk;
		if (clk_tmp) {
			clk_disable(clk_tmp);
			clk_unprepare(clk_tmp);
		}
	} else {
		;
	}

	clk_tmp = hisifd->dss_pri_clk;
	if (clk_tmp) {
		clk_disable(clk_tmp);
		clk_unprepare(clk_tmp);
	}

	return 0;
}

/*******************************************************************************
 **
 */
static int dpe_on(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	dpe_common_clk_enable(hisifd);
	dpe_inner_clk_enable(hisifd);
	/*DSS regulator are already enabled in fastboot, kernel don't care */
	/*dpe_regulator_enable(hisifd); */

	dss_inner_clk_common_enable(hisifd, false);
	if (hisifd->index == PRIMARY_PANEL_IDX) {
		dss_inner_clk_pdp_enable(hisifd, false);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		dss_inner_clk_sdp_enable(hisifd);
	} else {
		;
	}

	dpe_init(hisifd, false);
	if (dpe_recover_pxl_clock(hisifd)) {
		HISI_FB_ERR
		    ("fb%d failed to recover pixel clock which is larger than 288M!\n",
		     hisifd->index);
		return -EINVAL;
	}

	if (is_ldi_panel(hisifd)) {
		hisifd->panel_info.lcd_init_step = LCD_INIT_POWER_ON;
		ret = panel_next_on(pdev);
		if (ret) {
			HISI_FB_ERR("fb%d failed ret %d\n", hisifd->index, ret);
			return -EINVAL;
		}
	}

	ret = panel_next_on(pdev);
	if (hisifd->panel_info.vsync_ctrl_type == VSYNC_CTRL_NONE) {
		dpe_interrupt_mask(hisifd);
		dpe_interrupt_clear(hisifd);
		dpe_irq_enable(hisifd);
		dpe_interrupt_unmask(hisifd);
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int dpe_off(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	if (hisifd->panel_info.vsync_ctrl_type == VSYNC_CTRL_NONE) {
		dpe_interrupt_mask(hisifd);
		dpe_irq_disable(hisifd);
	} else {
		if (hisifd->vsync_ctrl.vsync_ctrl_enabled == 1) {
			if (hisifd->panel_info.
			    vsync_ctrl_type & VSYNC_CTRL_ISR_OFF) {
				dpe_interrupt_mask(hisifd);
				dpe_irq_disable(hisifd);
				HISI_FB_INFO
				    ("fb%d, need to disable dpe irq! vsync_ctrl_enabled=%d.\n",
				     hisifd->index,
				     hisifd->vsync_ctrl.vsync_ctrl_enabled);
			}
		}
	}

	ret = panel_next_off(pdev);

	dpe_deinit(hisifd);
	dpe_check_itf_status(hisifd);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		dss_inner_clk_pdp_disable(hisifd);
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		dss_inner_clk_sdp_disable(hisifd);
	} else {
		;
	}
	dss_inner_clk_common_disable(hisifd);

	/*dpe_regulator_disable(hisifd); */
	dpe_inner_clk_disable(hisifd);
	dpe_common_clk_disable(hisifd);

	if (hisifd->vsync_ctrl_type != VSYNC_CTRL_NONE) {
		if (!is_dss_idle_enable())
			hisifd->panel_info.vsync_ctrl_type = VSYNC_CTRL_NONE;
		else
			hisifd->panel_info.vsync_ctrl_type =
			    hisifd->vsync_ctrl_type;
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int dpe_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = panel_next_remove(pdev);

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int dpe_set_backlight(struct platform_device *pdev, uint32_t bl_level)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);
	pinfo = &(hisifd->panel_info);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	HISI_FB_DEBUG("fb%d, bl_level=%d.\n", hisifd->index, bl_level);

	if (pinfo->bl_max < 1) {
		HISI_FB_ERR("bl_max(%d) is out of range!!", pinfo->bl_max);
		return -EINVAL;
	}

	if (bl_level > pinfo->bl_max) {
		bl_level = pinfo->bl_max;
	}

	if (bl_level < pinfo->bl_min && bl_level) {
		bl_level = pinfo->bl_min;
	}

	ret = panel_next_set_backlight(pdev, bl_level);

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int dpe_vsync_ctrl(struct platform_device *pdev, int enable)
{
	int ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	if (enable) {
		ret = panel_next_vsync_ctrl(pdev, enable);
		if (hisifd->panel_info.vsync_ctrl_type & VSYNC_CTRL_ISR_OFF) {
			dpe_interrupt_mask(hisifd);
			dpe_interrupt_clear(hisifd);
			dpe_irq_enable(hisifd);
			dpe_interrupt_unmask(hisifd);
		}
	} else {
		ret = panel_next_vsync_ctrl(pdev, enable);
		if (hisifd->panel_info.vsync_ctrl_type & VSYNC_CTRL_ISR_OFF) {
			dpe_interrupt_mask(hisifd);
			dpe_interrupt_clear(hisifd);
			dpe_irq_disable_nosync(hisifd);
		}
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static int dpe_regulator_clk_irq_setup(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;
	struct dss_clk_rate *pdss_clk_rate = NULL;
	const char *irq_name = NULL;
	irqreturn_t(*isr_fnc)(int irq, void *ptr);
	int ret = 0;
	uint64_t pxl_clk_rate = 0;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	pinfo = &(hisifd->panel_info);
	pdss_clk_rate = get_dss_clk_rate(hisifd);
	BUG_ON(pdss_clk_rate == NULL);

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		irq_name = IRQ_PDP_NAME;
		isr_fnc = dss_pdp_isr;
	} else if (hisifd->index == EXTERNAL_PANEL_IDX) {
		irq_name = IRQ_SDP_NAME;
		isr_fnc = dss_sdp_isr;
	} else if (hisifd->index == AUXILIARY_PANEL_IDX) {
		irq_name = IRQ_ADP_NAME;
		isr_fnc = dss_adp_isr;
	} else {
		HISI_FB_ERR("fb%d, not support this device!\n", hisifd->index);
		return -EINVAL;
	}

	HISI_FB_INFO("dss_pclk_dss_clk:[%llu]->[%llu].\n",
		     pdss_clk_rate->dss_pclk_dss_rate,
		     (uint64_t) clk_get_rate(hisifd->dss_pclk_dss_clk));

	ret =
	    clk_set_rate(hisifd->dss_pri_clk, pdss_clk_rate->dss_pri_clk_rate);
	if (ret < 0) {
		HISI_FB_ERR
		    ("fb%d dss_pri_clk clk_set_rate(%llu) failed, error=%d!\n",
		     hisifd->index, pdss_clk_rate->dss_pri_clk_rate, ret);
		return -EINVAL;
	}
	HISI_FB_INFO("dss_pri_clk:[%llu]->[%llu].\n",
		     pdss_clk_rate->dss_pri_clk_rate,
		     (uint64_t) clk_get_rate(hisifd->dss_pri_clk));

	if (hisifd->index == PRIMARY_PANEL_IDX) {
		pxl_clk_rate =
		    (pinfo->pxl_clk_rate >
		     DSS_MAX_PXL0_CLK_288M) ? DSS_MAX_PXL0_CLK_288M : pinfo->
		    pxl_clk_rate;
		if (pinfo->pxl_clk_rate_adjust > 0) {
			ret =
			    clk_set_rate(hisifd->dss_pxl0_clk,
					 pinfo->pxl_clk_rate_adjust);
		} else {
			ret = clk_set_rate(hisifd->dss_pxl0_clk, pxl_clk_rate);
		}

		if (ret < 0) {
			HISI_FB_ERR
			    ("fb%d dss_pxl0_clk clk_set_rate(%llu) failed, error=%d!\n",
			     hisifd->index, pinfo->pxl_clk_rate, ret);
		}
		HISI_FB_INFO("dss_pxl0_clk:[%llu]->[%llu].\n",
			     pinfo->pxl_clk_rate,
			     (uint64_t) clk_get_rate(hisifd->dss_pxl0_clk));
	} else if ((hisifd->index == EXTERNAL_PANEL_IDX)
		   && !hisifd->panel_info.fake_hdmi) {
		ret = clk_set_rate(hisifd->dss_pxl1_clk, pinfo->pxl_clk_rate);
		if (ret < 0) {
			HISI_FB_ERR
			    ("fb%d dss_pxl1_clk clk_set_rate(%llu) failed, error=%d!\n",
			     hisifd->index, pinfo->pxl_clk_rate, ret);
		}
		HISI_FB_INFO("dss_pxl1_clk:[%llu]->[%llu].\n",
			     pinfo->pxl_clk_rate,
			     (uint64_t) clk_get_rate(hisifd->dss_pxl1_clk));
	} else {
		;
	}

	if (hisifd->dpe_irq) {
		ret =
		    request_irq(hisifd->dpe_irq, isr_fnc, 0, irq_name,
				(void *)hisifd);
		if (ret != 0) {
			HISI_FB_ERR
			    ("fb%d request_irq failed, irq_no=%d error=%d!\n",
			     hisifd->index, hisifd->dpe_irq, ret);
			return ret;
		} else {
			disable_irq(hisifd->dpe_irq);
		}
	}
	return 0;
}

static int dpe_probe(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct platform_device *hisi_fb_dev = NULL;
	struct hisi_fb_panel_data *pdata = NULL;
	struct fb_info *fbi = NULL;
	int ret = 0;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = dpe_regulator_clk_irq_setup(pdev);
	if (ret) {
		HISI_FB_ERR("fb%d dpe_irq_clk_setup failed, error=%d!\n",
			    hisifd->index, ret);
		goto err;
	}

	/* alloc device */
	hisi_fb_dev = platform_device_alloc(DEV_NAME_FB, pdev->id);
	if (!hisi_fb_dev) {
		HISI_FB_ERR("fb%d platform_device_alloc failed, error=%d!\n",
			    hisifd->index, ret);
		ret = -ENOMEM;
		goto err_device_alloc;
	}

	/* link to the latest pdev */
	hisifd->pdev = hisi_fb_dev;

	/* alloc panel device data */
	ret =
	    platform_device_add_data(hisi_fb_dev, dev_get_platdata(&pdev->dev),
				     sizeof(struct hisi_fb_panel_data));
	if (ret) {
		HISI_FB_ERR("fb%d platform_device_add_data failed, error=%d!\n",
			    hisifd->index, ret);
		goto err_device_put;
	}

	/* data chain */
	pdata = dev_get_platdata(&hisi_fb_dev->dev);
	pdata->on = dpe_on;
	pdata->off = dpe_off;
	pdata->remove = dpe_remove;
	pdata->set_backlight = dpe_set_backlight;
	pdata->vsync_ctrl = dpe_vsync_ctrl;
	pdata->next = pdev;

	/* get/set panel info */
	memcpy(&hisifd->panel_info, pdata->panel_info,
	       sizeof(struct hisi_panel_info));

	fbi = hisifd->fbi;
	fbi->var.pixclock = hisifd->panel_info.pxl_clk_rate;
	/*fbi->var.pixclock = clk_round_rate(hisifd->dpe_clk,
			hisifd->panel_info.pxl_clk_rate); */
	fbi->var.left_margin = hisifd->panel_info.ldi.h_back_porch;
	fbi->var.right_margin = hisifd->panel_info.ldi.h_front_porch;
	fbi->var.upper_margin = hisifd->panel_info.ldi.v_back_porch;
	fbi->var.lower_margin = hisifd->panel_info.ldi.v_front_porch;
	fbi->var.hsync_len = hisifd->panel_info.ldi.h_pulse_width;
	fbi->var.vsync_len = hisifd->panel_info.ldi.v_pulse_width;

	hisifd->vsync_ctrl_type = hisifd->panel_info.vsync_ctrl_type;

	/* set driver data */
	platform_set_drvdata(hisi_fb_dev, hisifd);
	ret = platform_device_add(hisi_fb_dev);
	if (ret) {
		HISI_FB_ERR("fb%d platform_device_add failed, error=%d!\n",
			    hisifd->index, ret);
		goto err_device_put;
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return 0;

 err_device_put:
	platform_device_put(hisi_fb_dev);
 err_device_alloc:
 err:
	return ret;
}

static struct platform_driver this_driver = {
	.probe = dpe_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = DEV_NAME_DSS_DPE,
		   },
};

static int __init dpe_driver_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		HISI_FB_ERR("platform_driver_register failed, error=%d!\n",
			    ret);
		return ret;
	}

	return ret;
}

module_init(dpe_driver_init);

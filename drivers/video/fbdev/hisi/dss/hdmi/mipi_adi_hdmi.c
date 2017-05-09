/* Copyright (c) 2008-2011, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above
 *	   copyright notice, this list of conditions and the following
 *	   disclaimer in the documentation and/or other materials provided
 *	   with the distribution.
 *	 * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "../hisi_fb.h"
#include "adv75xx.h"

/*******************************************************************************
 **
 */
static int mipi_adi_hdmi_on(struct platform_device *pdev)
{
	struct adi_hdmi *adv75xx = NULL;
	struct hisi_fb_panel_data *pdata = NULL;
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;

	HISI_FB_INFO("+.\n");

	if (pdev == NULL) {
		HISI_FB_ERR("pdev is NULL!\n");
		return -1;
	}

	HISI_FB_INFO("pdev->name = %s, pdev->id = %d\n", pdev->name, pdev->id);

	hisifd = platform_get_drvdata(pdev);
	if (hisifd == NULL) {
		HISI_FB_ERR("platform get drivre data failed!!\n");
		return -1;
	}

	HISI_FB_INFO("fb%d, +!\n", hisifd->index);

	pinfo = &(hisifd->panel_info);

	pdata = dev_get_platdata(&pdev->dev);
	if (pdata == NULL) {
		HISI_FB_ERR("devices get platform data failed!!\n");
		return -1;
	}

	if (pdata->next) {
		adv75xx = platform_get_drvdata(pdata->next);
		if (!adv75xx) {
			HISI_FB_ERR("platform get drivre data failed!\n");
			return -1;
		}
	} else {
		HISI_FB_ERR("pdata->next is NULL!!\n");
		return -1;
	}

	HISI_FB_INFO("adv75xx->i2c_main->name is %s!\n",
		     adv75xx->i2c_main->name);
	HISI_FB_INFO("adv75xx->mode->vdisplay is %d!\n",
		     adv75xx->mode->vdisplay);

	if (pinfo->lcd_init_step == LCD_INIT_POWER_ON) {
		pinfo->lcd_init_step = LCD_INIT_MIPI_LP_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_LP_SEND_SEQUENCE) {
		pinfo->lcd_init_step = LCD_INIT_MIPI_HS_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_HS_SEND_SEQUENCE) {
		adv75xx->opt_funcs->mode_set(adv75xx, adv75xx->mode);
		adv75xx->opt_funcs->power_on(adv75xx);
	} else {
		HISI_FB_ERR("failed to init hdmi!\n");
	}

	HISI_FB_INFO("-.\n");

	return 0;
}

static int mipi_adi_hdmi_off(struct platform_device *pdev)
{
	struct adi_hdmi *adv75xx = NULL;
	struct hisi_fb_panel_data *pdata = NULL;

	HISI_FB_INFO("+.\n");

	BUG_ON(pdev == NULL);
	pdata = dev_get_platdata(&pdev->dev);
	BUG_ON(pdata == NULL);

	HISI_FB_INFO("pdev->name = %s, pdev->id = %d +.\n", pdev->name,
		     pdev->id);

	if (pdata->next) {
		adv75xx = platform_get_drvdata(pdata->next);
		if (!adv75xx) {
			HISI_FB_ERR("platform get drivre data failed!\n");
			return -1;
		}
	}

	HISI_FB_INFO("adv75xx->i2c_main->name is %s!\n",
		     adv75xx->i2c_main->name);
	HISI_FB_INFO("adv75xx->mode->vdisplay is %d!\n",
		     adv75xx->mode->vdisplay);

	adv75xx->opt_funcs->power_off(adv75xx);

	HISI_FB_INFO("-.\n");

	return 0;
}

static int mipi_adi_hdmi_remove(struct platform_device *pdev)
{

	return 0;
}

/*******************************************************************************
 **
 */
static struct hisi_panel_info g_adi_hdmi_info = { 0 };

static struct hisi_fb_panel_data g_adi_hdmi_data = {
	.panel_info = &g_adi_hdmi_info,
	.on = mipi_adi_hdmi_on,
	.off = mipi_adi_hdmi_off,
};

/*******************************************************************************
 **
 */
static int mipi_adi_hdmi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct adi_hdmi *adv75xx = NULL;
	struct hisi_panel_info *pinfo = NULL;
	struct hisi_display_mode *mode = NULL;

	if (pdev == NULL)
		HISI_FB_ERR("platform device is NULL!\n");

	HISI_FB_INFO("pdev->name = %s, pdev->id = %d +.\n", pdev->name,
		     pdev->id);

	adv75xx = platform_get_drvdata(pdev);
	if (!adv75xx) {
		HISI_FB_ERR("platform get drivre data failed!\n");
		goto err_probe_defer;
	}

	HISI_FB_INFO("adv75xx->i2c_main->name is %s!\n",
		     adv75xx->i2c_main->name);

	HISI_FB_INFO("adv75xx->mode->vdisplay is %d!\n",
		     adv75xx->mode->vdisplay);

	if (adv75xx->mode) {
		mode = adv75xx->mode;
		/* init hdmi display info */
		pinfo = g_adi_hdmi_data.panel_info;
		pinfo->xres = mode->hdisplay;
		pinfo->yres = mode->vdisplay;
		pinfo->width = mode->width_mm;
		pinfo->height = mode->height_mm;
		pinfo->orientation = LCD_PORTRAIT;
		pinfo->bpp = LCD_RGB888;
		pinfo->bgr_fmt = LCD_RGB;
		pinfo->bl_set_type = BL_SET_BY_MIPI;

		pinfo->type = PANEL_MIPI_VIDEO;

		pinfo->bl_min = 1;
		pinfo->bl_max = 255;
		pinfo->bl_default = 102;

		pinfo->pxl_clk_rate = mode->clock * 1000UL;
		pinfo->ldi.h_back_porch = mode->htotal - mode->hsync_end;
		pinfo->ldi.h_front_porch = mode->hsync_offset;
		pinfo->ldi.h_pulse_width = mode->hsync_pulse_width;
		pinfo->ldi.v_back_porch = mode->vtotal - mode->vsync_end;
		pinfo->ldi.v_front_porch = mode->vsync_offset;
		pinfo->ldi.v_pulse_width = mode->vsync_pulse_width;
	} else {
		/* init hdmi display info */
		pinfo = g_adi_hdmi_data.panel_info;
		pinfo->xres = 1920;
		pinfo->yres = 1080;
		pinfo->width = 16000;
		pinfo->height = 9000;

		pinfo->orientation = LCD_PORTRAIT;
		pinfo->bpp = LCD_RGB888;
		pinfo->bgr_fmt = LCD_RGB;
		pinfo->bl_set_type = BL_SET_BY_MIPI;

		pinfo->type = PANEL_MIPI_VIDEO;

		pinfo->bl_min = 1;
		pinfo->bl_max = 255;
		pinfo->bl_default = 102;

		pinfo->ldi.h_back_porch = 148;
		pinfo->ldi.h_front_porch = 88;
		pinfo->ldi.h_pulse_width = 44;
		pinfo->ldi.v_back_porch = 36;
		pinfo->ldi.v_front_porch = 4;
		pinfo->ldi.v_pulse_width = 5;
	}


	pinfo->mipi.dsi_bit_clk = 432;


	pinfo->dsi_bit_clk_upt_support = 0;
	pinfo->mipi.dsi_bit_clk_upt = pinfo->mipi.dsi_bit_clk;

	pinfo->mipi.non_continue_en = 0;

	pinfo->pxl_clk_rate = 144 * 1000000UL;


	pinfo->mipi.lane_nums = DSI_4_LANES;
	pinfo->mipi.color_mode = DSI_24BITS_1;
	pinfo->mipi.vc = 0;
	pinfo->mipi.max_tx_esc_clk = 10 * 1000000;
	pinfo->mipi.burst_mode = DSI_NON_BURST_SYNC_PULSES;

	pinfo->mipi.clk_post_adjust = 120;
	pinfo->mipi.clk_pre_adjust = 0;
	pinfo->mipi.clk_t_hs_prepare_adjust = 0;
	pinfo->mipi.clk_t_lpx_adjust = 0;
	pinfo->mipi.clk_t_hs_trial_adjust = 0;
	pinfo->mipi.clk_t_hs_exit_adjust = 0;
	pinfo->mipi.clk_t_hs_zero_adjust = 0;

	pinfo->pxl_clk_rate_div = 1;

	g_adi_hdmi_data.next = pdev;
	HISI_FB_INFO("The pixel clock is %llu !\n", pinfo->pxl_clk_rate);
	HISI_FB_INFO("The resolution is %d x %d !\n", pinfo->xres, pinfo->yres);
	HISI_FB_INFO
	    ("hsw = %d, hfp = %d, hbp = %d, vsw = %d, vfp= %d, vbp = %d\n",
	     pinfo->ldi.h_pulse_width, pinfo->ldi.h_front_porch,
	     pinfo->ldi.h_back_porch, pinfo->ldi.v_pulse_width,
	     pinfo->ldi.v_front_porch, pinfo->ldi.v_back_porch);


	ret = platform_device_add_data(pdev, &g_adi_hdmi_data,
				       sizeof(struct hisi_fb_panel_data));
	if (ret) {
		HISI_FB_ERR("platform_device_add_data failed!\n");
		goto err_device_put;
	}

	hisi_fb_add_device(pdev);

	HISI_FB_INFO("-.\n");

	return 0;

 err_device_put:
	platform_device_put(pdev);
 err_probe_defer:
	return -EPROBE_DEFER;
}

static struct platform_driver this_driver = {
	.probe = mipi_adi_hdmi_probe,
	.remove = mipi_adi_hdmi_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "adi_hdmi",
		   }
};

static int __init mipi_adi_hdmi_init(void)
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

module_init(mipi_adi_hdmi_init);

/* Copyright (c) 2008-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
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

#include "../hisi_fb.h"

#define DTS_COMP_MIPI_HIKEY	"hisilicon,mipi_hikey"

/********************************Hikey start***********************
 **Power ON Sequence(sleep mode to Normal mode)
 */
static char hikey_power_on_param1[] = {
	0x01,
};

static char hikey_power_on_param2[] = {
	0xB0,
	0x00,
};

static char hikey_power_on_param3[] = {
	0xD6,
	0x01,
};

static char hikey_power_on_param4[] = {
	0xB3,
	0x14, 0x08, 0x00, 0x22, 0x00,
};

static char hikey_power_on_param5[] = {
	0xB4,
	0x0C,
};

static char hikey_power_on_param6[] = {
	0xB6,
	0x3A, 0xC3,
};

static char hikey_power_on_param7[] = {
	0x2A,
	0x00, 0x00, 0X04, 0XAF,
};

static char hikey_power_on_param8[] = {
	0x2B,
	0x00, 0x00, 0X07, 0X7F,
};

static char hikey_power_on_param9[] = {
	0x51,
	0xA6,
};

static char hikey_power_on_param10[] = {
	0x53,
	0x2C,
};

static char hikey_power_on_param11[] = {
	0x3A,
	0x66,
};

static char hikey_power_on_param12[] = {
	0x29,
};

static char hikey_power_on_param13[] = {
	0x11,
};

static char hikey_display_off[] = {
	0x28,
};

static char hikey_enter_sleep[] = {
	0x10,
};

static struct dsi_cmd_desc hikey_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 0, 20, WAIT_TYPE_MS,
	 sizeof(hikey_display_off), hikey_display_off}
	,
	{DTYPE_DCS_WRITE, 0, 80, WAIT_TYPE_MS,
	 sizeof(hikey_enter_sleep), hikey_enter_sleep}
	,
};

/*short or long packet*/
static struct dsi_cmd_desc hikey_display_on_cmds[] = {
	{DTYPE_DCS_WRITE, 0, 5, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param1), hikey_power_on_param1}
	,
	{DTYPE_DCS_WRITE1, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param2), hikey_power_on_param2}
	,
	{DTYPE_DCS_WRITE1, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param3), hikey_power_on_param3}
	,
	{DTYPE_DCS_LWRITE, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param4), hikey_power_on_param4}
	,
	{DTYPE_DCS_WRITE1, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param5), hikey_power_on_param5}
	,
	{DTYPE_DCS_LWRITE, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param6), hikey_power_on_param6}
	,
	{DTYPE_DCS_LWRITE, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param7), hikey_power_on_param7}
	,
	{DTYPE_DCS_LWRITE, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param8), hikey_power_on_param8}
	,
	{DTYPE_DCS_WRITE1, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param9), hikey_power_on_param9}
	,
	{DTYPE_DCS_WRITE1, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param10), hikey_power_on_param10}
	,
	{DTYPE_DCS_WRITE1, 0, 2, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param11), hikey_power_on_param11}
	,
	{DTYPE_DCS_WRITE, 0, 20, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param12), hikey_power_on_param12}
	,
	{DTYPE_DCS_WRITE, 0, 150, WAIT_TYPE_MS,
	 sizeof(hikey_power_on_param13), hikey_power_on_param13}
	,
};

/********************************hikey end*************************/

/*******************************************************************************
 ** LCD GPIO
 */
#define GPIO_LCD_PWR_ENABLE_NAME "gpio_lcd_pwr_enable"
#define GPIO_LCD_BL_ENABLE_NAME "gpio_lcd_bl_enable"
#define GPIO_LCD_PWM_NAME "gpio_lcd_pwm"
#define GPIO_SWITCH_DSI_HDMI "gpio_switch_dsi_hdmi"

static uint32_t gpio_lcd_pwr_enable;
static uint32_t gpio_lcd_bl_enable;
static uint32_t gpio_lcd_pwm;
static uint32_t gpio_switch_dsi_hdmi;

static struct gpio_desc hikey_lcd_gpio_request_cmds[] = {
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWR_ENABLE_NAME, &gpio_lcd_pwr_enable, 0},
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
	 GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 0},
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWM_NAME, &gpio_lcd_pwm, 0},
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
	 GPIO_SWITCH_DSI_HDMI, &gpio_switch_dsi_hdmi, 0},
};

static struct gpio_desc hikey_lcd_gpio_free_cmds[] = {
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWR_ENABLE_NAME, &gpio_lcd_pwr_enable, 0},
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
	 GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 0},
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWM_NAME, &gpio_lcd_pwm, 0},
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
	 GPIO_SWITCH_DSI_HDMI, &gpio_switch_dsi_hdmi, 0},
};

static struct gpio_desc hikey_lcd_gpio_normal_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWR_ENABLE_NAME, &gpio_lcd_pwr_enable, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_SWITCH_DSI_HDMI, &gpio_switch_dsi_hdmi, 1},
};

static struct gpio_desc hikey_lcd_gpio_off_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWR_ENABLE_NAME, &gpio_lcd_pwr_enable, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_SWITCH_DSI_HDMI, &gpio_switch_dsi_hdmi, 0},
};

static struct gpio_desc hikey_lcd_backlight_enable_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWM_NAME, &gpio_lcd_pwm, 1},
};

static struct gpio_desc hikey_lcd_backlight_disable_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 0,
	 GPIO_LCD_PWM_NAME, &gpio_lcd_pwm, 0},
};

static struct hisi_fb_panel_data g_panel_data;

static void hikey_set_backlight_on(void)
{
	msleep(200);
	gpio_cmds_tx(hikey_lcd_backlight_enable_cmds,
		     ARRAY_SIZE(hikey_lcd_backlight_enable_cmds));
	return;
}

static void hikey_set_backlight_off(void)
{
	gpio_cmds_tx(hikey_lcd_backlight_disable_cmds,
		     ARRAY_SIZE(hikey_lcd_backlight_disable_cmds));
	return;
}

static int hikey_panel_on(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;
	char __iomem *mipi_dsi0_base = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);
	pinfo = &(hisifd->panel_info);
	BUG_ON(pinfo == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	mipi_dsi0_base = hisifd->mipi_dsi0_base;

	if (pinfo->lcd_init_step == LCD_INIT_POWER_ON) {
		pinfo->lcd_init_step = LCD_INIT_MIPI_LP_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_LP_SEND_SEQUENCE) {
		/*lcd gpio request */
		gpio_cmds_tx(hikey_lcd_gpio_request_cmds,
			     ARRAY_SIZE(hikey_lcd_gpio_request_cmds));
		/*lcd gpio normal */
		gpio_cmds_tx(hikey_lcd_gpio_normal_cmds,
			     ARRAY_SIZE(hikey_lcd_gpio_normal_cmds));
		/*lcd display on sequence */
		msleep(250);
		mipi_dsi_cmds_tx(hikey_display_on_cmds,
				 ARRAY_SIZE(hikey_display_on_cmds),
				 mipi_dsi0_base);

		pinfo->lcd_init_step = LCD_INIT_MIPI_HS_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_HS_SEND_SEQUENCE) {
		;
	} else {
		HISI_FB_ERR("failed to init lcd!\n");
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return 0;
}

static int hikey_panel_off(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;
	char __iomem *mipi_dsi0_base = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);
	pinfo = &(hisifd->panel_info);
	BUG_ON(pinfo == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);
	mipi_dsi0_base = hisifd->mipi_dsi0_base;
	/*lcd enter sleep */
	mipi_dsi_cmds_tx(hikey_display_off_cmds,
			 ARRAY_SIZE(hikey_display_off_cmds), mipi_dsi0_base);
	gpio_cmds_tx(hikey_lcd_gpio_off_cmds,
		     ARRAY_SIZE(hikey_lcd_gpio_off_cmds));
	gpio_cmds_tx(hikey_lcd_gpio_free_cmds,
		     ARRAY_SIZE(hikey_lcd_gpio_free_cmds));
	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return 0;
}

static int hikey_panel_remove(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);
	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return 0;
}

static int hikey_panel_set_backlight(struct platform_device *pdev,
				     uint32_t bl_level)
{
	struct hisi_fb_data_type *hisifd = NULL;
	int ret = 0;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	if (bl_level == 0) {
		hikey_set_backlight_off();
	} else {
		hikey_set_backlight_on();
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static struct hisi_panel_info g_panel_info = { 0 };

static struct hisi_fb_panel_data g_panel_data = {
	.panel_info = &g_panel_info,
	.on = hikey_panel_on,
	.off = hikey_panel_off,
	.remove = hikey_panel_remove,
	.set_backlight = hikey_panel_set_backlight,
};

static int hikey_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_panel_info *pinfo = NULL;
	struct device_node *np = NULL;
	uint32_t bl_type = 0;

	uint32_t lcd_display_type = 0;
	uint32_t lcd_ifbc_type = 0;

	HISI_FB_DEBUG("+.\n");

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_MIPI_HIKEY);
	if (!np) {
		HISI_FB_ERR("NOT FOUND device node %s!\n", DTS_COMP_MIPI_HIKEY);
		goto err_return;
	}

	ret = of_property_read_u32(np, LCD_BL_TYPE_NAME, &bl_type);
	if (ret) {
		HISI_FB_ERR("get lcd_bl_type failed!\n");
		bl_type = BL_SET_BY_BLPWM;
	}

	ret =
	    of_property_read_u32(np, LCD_DISPLAY_TYPE_NAME, &lcd_display_type);
	if (ret) {
		HISI_FB_ERR("get lcd_display_type failed!\n");
		lcd_display_type = PANEL_MIPI_VIDEO;
	}

	ret = of_property_read_u32(np, LCD_IFBC_TYPE_NAME, &lcd_ifbc_type);
	if (ret) {
		HISI_FB_ERR("get ifbc_type failed!\n");
		lcd_ifbc_type = IFBC_TYPE_NONE;
	}

	/*GPIO_26_8 GPIO_216 */
	gpio_lcd_pwr_enable = of_get_named_gpio(np, "gpios", 0);
	/*GPIO_27_2 GPIO_218 */
	gpio_lcd_bl_enable = of_get_named_gpio(np, "gpios", 1);
	/*GPIO_22_6 GPIO_182 */
	gpio_lcd_pwm = of_get_named_gpio(np, "gpios", 2);
	/*GPIO_2_4 GPIO_020 */
	gpio_switch_dsi_hdmi = of_get_named_gpio(np, "gpios", 3);

	if (hisi_fb_device_probe_defer(lcd_display_type, bl_type)) {
		goto err_probe_defer;
	}

	pdev->id = 1;
	/*init lcd panel info */
	pinfo = g_panel_data.panel_info;
	memset(pinfo, 0, sizeof(struct hisi_panel_info));
	pinfo->xres = 1200;
	pinfo->yres = 1920;
	pinfo->width = 94;
	pinfo->height = 151;
	pinfo->orientation = LCD_PORTRAIT;
	pinfo->bpp = LCD_RGB888;
	pinfo->bgr_fmt = LCD_RGB;
	pinfo->bl_set_type = bl_type;

	pinfo->type = PANEL_MIPI_VIDEO;
	pinfo->ifbc_type = 0;

	if (pinfo->bl_set_type == BL_SET_BY_BLPWM)
		pinfo->blpwm_input_ena = 0;

	pinfo->bl_min = 1;
	pinfo->bl_max = 255;
	pinfo->bl_default = 102;
	pinfo->esd_enable = 0;

	/*ldi */
	pinfo->ldi.h_back_porch = 60;
	pinfo->ldi.h_front_porch = 200;
	pinfo->ldi.h_pulse_width = 12;
	pinfo->ldi.v_back_porch = 8;
	pinfo->ldi.v_front_porch = 8;
	pinfo->ldi.v_pulse_width = 2;

	/*
	   pinfo->ldi.hsync_plr = 0;
	   pinfo->ldi.vsync_plr = 0;
	   pinfo->ldi.pixelclk_plr = 1;
	   pinfo->ldi.data_en_plr = 0;
	 */

	/*mipi */
	pinfo->mipi.lane_nums = DSI_4_LANES;
	pinfo->mipi.color_mode = DSI_24BITS_1;
	pinfo->mipi.vc = 0;
	pinfo->mipi.max_tx_esc_clk = 10 * 1000000;
	pinfo->mipi.burst_mode = DSI_BURST_SYNC_PULSES_1;

	pinfo->mipi.dsi_bit_clk = 480;
	pinfo->mipi.dsi_bit_clk_upt = pinfo->mipi.dsi_bit_clk;

	pinfo->pxl_clk_rate = 146 * 1000000UL;
	pinfo->pxl_clk_rate_div = 1;
	pinfo->fps = 50;

	pinfo->vsync_ctrl_type = 0;
	pinfo->dirty_region_updt_support = 0;
	pinfo->dsi_bit_clk_upt_support = 0;

	/*alloc panel device data */
	ret = platform_device_add_data(pdev, &g_panel_data,
				       sizeof(struct hisi_fb_panel_data));
	if (ret) {
		HISI_FB_ERR("platform_device_add_data failed!\n");
		goto err_device_put;
	}

	hisi_fb_add_device(pdev);

	/*
	   vdd = devm_regulator_get(&(pdev->dev), "vdd");
	   if (IS_ERR(vdd)) {
	   ret = PTR_ERR(vdd);
	   HISI_FB_ERR("vdd regulator get fail\n");
	   return ret;
	   }

	   ret = regulator_set_voltage(vdd, 1800000, 1800000);
	   if (ret) {
	   HISI_FB_ERR("vdd regulator set voltage fail\n");
	   return ret;
	   }

	   ret = regulator_enable(vdd);
	   if (ret) {
	   HISI_FB_ERR("vdd regulator enable fail\n");
	   return ret;
	   }
	 */

	HISI_FB_DEBUG("-.\n");
	return 0;

 err_device_put:
	platform_device_put(pdev);
 err_return:
	return ret;
 err_probe_defer:
	return -EPROBE_DEFER;
}

static const struct of_device_id hisi_panel_match_table[] = {
	{
	 .compatible = DTS_COMP_MIPI_HIKEY,
	 .data = NULL,
	 },
	{},
};

static struct platform_driver this_driver = {
	.probe = hikey_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "mipi_hikey",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(hisi_panel_match_table),
		   },
};

static int __init hikey_panel_init(void)
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

module_init(hikey_panel_init);

/*
 * HiKey LCD panel driver
 * TODO: Add backlight adjustment support.
 *
 * Copyright (c) 2016 Linaro Limited.
 * Copyright (c) 2016 Hisilicon Limited.
 * Copyright (C) 2016 LeMaker
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <video/mipi_display.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#define REGFLAG_DELAY         	0XFFE

struct hikey_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	bool prepared;
	bool enabled;

	struct gpio_desc *gpio_pwr_en;
	struct gpio_desc *gpio_bl_en;
	struct gpio_desc *gpio_pwm;
};

struct dsi_panel_cmd {
	u32 cmd;	/* cmd: DCS command */
	u32 len;	/* command payload length */
	u8 data[64];	/* buffer containing the command payload */
};

static struct dsi_panel_cmd n070icn_init_cmds[] = {
	{0xFF,4,{0xAA,0x55,0xA5,0x80}},//========== Internal setting ==========

	{0x6F,2,{0x11,0x00}},// MIPI related Timing Setting
	{0xF7,2,{0x20,0x00}},

	{0x6F,1,{0x06}},//  Improve ESD option
	{0xF7,1,{0xA0}},
	{0x6F,1,{0x19}},
	{0xF7,1,{0x12}},
	{0xF4,1,{0x03}},

	{0x6F,1,{0x08}},// Vcom floating
	{0xFA,1,{0x40}},
	{0x6F,1,{0x11}},
	{0xF3,1,{0x01}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},//========== page0 relative ==========
	{0xC8,1,{0x80}},

	{0xB1,2,{0x6C,0x01}},// Set WXGA resolution

	{0xB6,1,{0x08}},// Set source output hold time

	{0x6F,1,{0x02}},//EQ control function
	{0xB8,1,{0x08}},

	{0xBB,2,{0x54,0x54}},// Set bias current for GOP and SOP

	{0xBC,2,{0x05,0x05}},// Inversion setting

	{0xC7,1,{0x01}},// zigzag setting

	{0xBD,5,{0x02,0xB0,0x0C,0x0A,0x00}},// DSP Timing Settings update for BIST

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},//========== page1 relative ==========

	{0xB0,2,{0x05,0x05}},// Setting AVDD, AVEE clamp
	{0xB1,2,{0x05,0x05}},

	{0xBC,2,{0x3A,0x01}},// VGMP, VGMN, VGSP, VGSN setting
	{0xBD,2,{0x3E,0x01}},

	{0xCA,1,{0x00}},// gate signal control

	{0xC0,1,{0x04}},// power IC control

	{0xB2,2,{0x00,0x00}},// VCL SET -2.5V

	{0xBE,1,{0x80}},// VCOM = -1.888V

	{0xB3,2,{0x19,0x19}},// Setting VGH=15V, VGL=-11V
	{0xB4,2,{0x12,0x12}},

	{0xB9,2,{0x24,0x24}},// power control for VGH, VGL
	{0xBA,2,{0x14,0x14}},

	{0xF0,5,{0x55,0xAA,0x52,0x08,0x02}},//========== page2 relative ==========

	{0xEE,1,{0x01}},//gamma setting
	{0xEF,4,{0x09,0x06,0x15,0x18}},//Gradient Control for Gamma Voltage

	{0xB0,6,{0x00,0x00,0x00,0x08,0x00,0x17}},
	{0x6F,1,{0x06}},
	{0xB0,6,{0x00,0x25,0x00,0x30,0x00,0x45}},
	{0x6F,1,{0x0C}},
	{0xB0,4,{0x00,0x56,0x00,0x7A}},
	{0xB1,6,{0x00,0xA3,0x00,0xE7,0x01,0x20}},
	{0x6F,1,{0x06}},
	{0xB1,6,{0x01,0x7A,0x01,0xC2,0x01,0xC5}},
	{0x6F,1,{0x0C}},
	{0xB1,4,{0x02,0x06,0x02,0x5F}},
	{0xB2,6,{0x02,0x92,0x02,0xD0,0x02,0xFC}},
	{0x6F,1,{0x06}},
	{0xB2,6,{0x03,0x35,0x03,0x5D,0x03,0x8B}},
	{0x6F,1,{0x0C}},
	{0xB2,4,{0x03,0xA2,0x03,0xBF}},
	{0xB3,4,{0x03,0xD2,0x03,0xFF}},

	//========== GOA relative ==========
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x06}},// PAGE6 : GOUT Mapping, VGLO select
	{0xB0,2,{0x00,0x17}},
	{0xB1,2,{0x16,0x15}},
	{0xB2,2,{0x14,0x13}},
	{0xB3,2,{0x12,0x11}},
	{0xB4,2,{0x10,0x2D}},
	{0xB5,2,{0x01,0x08}},
	{0xB6,2,{0x09,0x31}},
	{0xB7,2,{0x31,0x31}},
	{0xB8,2,{0x31,0x31}},
	{0xB9,2,{0x31,0x31}},
	{0xBA,2,{0x31,0x31}},
	{0xBB,2,{0x31,0x31}},
	{0xBC,2,{0x31,0x31}},
	{0xBD,2,{0x31,0x09}},
	{0xBE,2,{0x08,0x01}},
	{0xBF,2,{0x2D,0x10}},
	{0xC0,2,{0x11,0x12}},
	{0xC1,2,{0x13,0x14}},
	{0xC2,2,{0x15,0x16}},
	{0xC3,2,{0x17,0x00}},
	{0xE5,2,{0x31,0x31}},
	{0xC4,2,{0x00,0x17}},
	{0xC5,2,{0x16,0x15}},
	{0xC6,2,{0x14,0x13}},
	{0xC7,2,{0x12,0x11}},
	{0xC8,2,{0x10,0x2D}},
	{0xC9,2,{0x01,0x08}},
	{0xCA,2,{0x09,0x31}},
	{0xCB,2,{0x31,0x31}},
	{0xCC,2,{0x31,0x31}},
	{0xCD,2,{0x31,0x31}},
	{0xCE,2,{0x31,0x31}},
	{0xCF,2,{0x31,0x31}},
	{0xD0,2,{0x31,0x31}},
	{0xD1,2,{0x31,0x09}},
	{0xD2,2,{0x08,0x01}},
	{0xD3,2,{0x2D,0x10}},
	{0xD4,2,{0x11,0x12}},
	{0xD5,2,{0x13,0x14}},
	{0xD6,2,{0x15,0x16}},
	{0xD7,2,{0x17,0x00}},
	{0xE6,2,{0x31,0x31}},
	{0xD8,5,{0x00,0x00,0x00,0x00,0x00}},//VGL level select
	{0xD9,5,{0x00,0x00,0x00,0x00,0x00}},
	{0xE7,1,{0x00}},

	// PAGE3 :
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x03}},//gate timing control
	{0xB0,2,{0x20,0x00}},
	{0xB1,2,{0x20,0x00}},
	{0xB2,5,{0x05,0x00,0x42,0x00,0x00}},
	{0xB6,5,{0x05,0x00,0x42,0x00,0x00}},
	{0xBA,5,{0x53,0x00,0x42,0x00,0x00}},
	{0xBB,5,{0x53,0x00,0x42,0x00,0x00}},
	{0xC4,1,{0x40}},

	// gate CLK EQ
	// gate STV EQ

	// PAGE5 :
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x05}},
	{0xB0,2,{0x17,0x06}},
	{0xB8,1,{0x00}},
	{0xBD,5,{0x03,0x01,0x01,0x00,0x01}},
	{0xB1,2,{0x17,0x06}},
	{0xB9,2,{0x00,0x01}},
	{0xB2,2,{0x17,0x06}},
	{0xBA,2,{0x00,0x01}},
	{0xB3,2,{0x17,0x06}},
	{0xBB,2,{0x0A,0x00}},
	{0xB4,2,{0x17,0x06}},
	{0xB5,2,{0x17,0x06}},
	{0xB6,2,{0x14,0x03}},
	{0xB7,2,{0x00,0x00}},
	{0xBC,2,{0x02,0x01}},
	{0xC0,1,{0x05}},
	{0xC4,1,{0xA5}},
	{0xC8,2,{0x03,0x30}},
	{0xC9,2,{0x03,0x51}},
	{0xD1,5,{0x00,0x05,0x03,0x00,0x00}},
	{0xD2,5,{0x00,0x05,0x09,0x00,0x00}},
	{0xE5,1,{0x02}},
	{0xE6,1,{0x02}},
	{0xE7,1,{0x02}},
	{0xE9,1,{0x02}},
	{0xED,1,{0x33}},

	/* bist test mode
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
	{0xEF,2,{0x07,0xFF}},
	{0xEE,4,{0x87,0x78,0x02,0x40}},
	*/

	{0x11,0,{0x00}},
	{REGFLAG_DELAY, 120, {}},
	{0x29,0,{0x00}},
	{REGFLAG_DELAY, 20, {}},
};

static int hikey_panel_wirte_cmds(struct mipi_dsi_device *dsi,
				  struct dsi_panel_cmd *cmds,
				  u32 count)
{
	struct dsi_panel_cmd *cmd;
	int ret = 0;
	u32 i;

	for(i = 0; i < count; i++) {
		cmd = &cmds[i];
		switch (cmd->cmd) {
		case REGFLAG_DELAY:
			msleep(cmd->len);
			break;
		default:
			ret = mipi_dsi_dcs_write(dsi, cmd->cmd, cmd->data,
						 cmd->len);
		}
	}

	return ret;
}

static inline struct hikey_panel *to_hikey_panel(struct drm_panel *panel)
{
	return container_of(panel, struct hikey_panel, base);
}

static int hikey_panel_disable(struct drm_panel *p)
{
	struct hikey_panel *panel = to_hikey_panel(p);

	if (!panel->enabled)
		return 0;

	/* TODO: send panel off seq cmds */
	panel->enabled = false;

	return 0;
}

static int hikey_panel_unprepare(struct drm_panel *p)
{
	struct hikey_panel *panel = to_hikey_panel(p);

	if (!panel->prepared)
		return 0;

	panel->prepared = false;

	return 0;
}

static int hikey_panel_prepare(struct drm_panel *p)
{
	struct hikey_panel *panel = to_hikey_panel(p);
	int ret;

	if (panel->prepared)
		return 0;

	/*
	 * A minimum delay of 250ms is required after power-up until commands
	 * can be sent
	 */
	msleep(250);

	/* init the panel */
	ret = hikey_panel_wirte_cmds(panel->dsi, n070icn_init_cmds,
				     ARRAY_SIZE(n070icn_init_cmds));
	if (ret < 0)
		return ret;

	panel->prepared = true;

	return 0;
}

static int hikey_panel_enable(struct drm_panel *p)
{
	struct hikey_panel *panel = to_hikey_panel(p);

	if (panel->enabled)
		return 0;

	msleep(200);
	gpiod_set_value(panel->gpio_bl_en, 1);
	gpiod_set_value(panel->gpio_pwm, 1);

	panel->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 66800,

	.hdisplay = 800,
	.hsync_start = 800 + 40,
	.hsync_end = 800 + 40 + 4,
	.htotal = 800 + 40 + 4 + 40,

	.vdisplay = 1280,
	.vsync_start = 1280 + 10,
	.vsync_end = 1280 + 10 + 4,
	.vtotal = 1280 + 10 + 4 + 12,
};

static int hikey_panel_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  default_mode.hdisplay, default_mode.vdisplay,
			  default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 94;
	panel->connector->display_info.height_mm = 150;

	return 1;
}

static const struct drm_panel_funcs hikey_panel_funcs = {
	.get_modes = hikey_panel_get_modes,
	.enable = hikey_panel_enable,
	.disable = hikey_panel_disable,
	.prepare = hikey_panel_prepare,
	.unprepare = hikey_panel_unprepare,
};

static int hikey_panel_add(struct hikey_panel *panel)
{
	struct device *dev = &panel->dsi->dev;
	int ret;

	drm_panel_init(&panel->base);
	panel->base.funcs = &hikey_panel_funcs;
	panel->base.dev = dev;

	ret = drm_panel_add(&panel->base);
	if (ret)
		return ret;

	return 0;
}

static void hikey_panel_del(struct hikey_panel *panel)
{
	if (panel->base.dev)
		drm_panel_remove(&panel->base);
}

static int hikey_panel_parse_dt(struct hikey_panel *panel)
{
	struct device *dev = &panel->dsi->dev;

	panel->gpio_pwr_en =
		devm_gpiod_get_optional(dev, "pwr-en", GPIOD_OUT_HIGH);
	if (IS_ERR(panel->gpio_pwr_en))
		return PTR_ERR(panel->gpio_pwr_en);

	panel->gpio_bl_en =
		devm_gpiod_get_optional(dev, "bl-en", GPIOD_OUT_LOW);
	if (IS_ERR(panel->gpio_bl_en))
		return PTR_ERR(panel->gpio_bl_en);

	panel->gpio_pwm =
		devm_gpiod_get_optional(dev, "pwm", GPIOD_OUT_LOW);
	if (IS_ERR(panel->gpio_pwm))
		return PTR_ERR(panel->gpio_pwm);

	return 0;
}

static int hikey_panel_attach_dsi(struct mipi_dsi_device *dsi)
{
	int ret;

	dsi->phy_clock = 480000; /* in kHz */
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_VIDEO_HSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		DRM_ERROR("failed to attach dsi to host\n");
		return ret;
	}

	return 0;
}

static int hikey_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct hikey_panel *panel;
	int ret;

	DRM_INFO("hikey_panel_probe enter\n");

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->dsi = dsi;
	ret = hikey_panel_parse_dt(panel);
	if (ret)
		return ret;

	ret = hikey_panel_add(panel);
	if (ret)
		return ret;

	ret = hikey_panel_attach_dsi(dsi);
	if (ret){
		hikey_panel_del(panel);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, panel);

	DRM_INFO("hikey_panel_probe exit\n");
	return 0;
}

static int hikey_panel_remove(struct mipi_dsi_device *dsi)
{
	struct hikey_panel *panel = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = hikey_panel_disable(&panel->base);
	if (ret < 0)
		DRM_ERROR("failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		DRM_ERROR("failed to detach from DSI host: %d\n", ret);

	drm_panel_detach(&panel->base);
	hikey_panel_del(panel);

	return 0;
}

static void hikey_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct hikey_panel *panel = mipi_dsi_get_drvdata(dsi);

	hikey_panel_disable(&panel->base);
}

static const struct of_device_id panel_of_match[] = {
	{ .compatible = "innolux,n070icn-pb1", },
	{ }
};
MODULE_DEVICE_TABLE(of, panel_of_match);

static struct mipi_dsi_driver hikey_panel_driver = {
	.driver = {
		.name = "hikey-lcd-panel",
		.of_match_table = panel_of_match,
	},
	.probe = hikey_panel_probe,
	.remove = hikey_panel_remove,
	.shutdown = hikey_panel_shutdown,
};
module_mipi_dsi_driver(hikey_panel_driver);

MODULE_AUTHOR("support@lemaker.org");
MODULE_DESCRIPTION("INNOLUX N070ICN-PB1 (800x1280) video mode panel driver");
MODULE_LICENSE("GPL v2");

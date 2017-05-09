/*
 * HiKey LCD panel driver
 * TODO: Add backlight adjustment support.
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

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#define REGFLAG_DELAY	0XFFE

struct hikey_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	bool prepared;
	bool enabled;

	struct gpio_desc *gpio_pwr_en;
	struct gpio_desc *gpio_bl_en;
	struct gpio_desc *gpio_pwm;

	struct regulator *vdd;
};

struct dsi_panel_cmd {
	u32 cmd;	/* cmd: DCS command */
	u32 len;	/* command payload length */
	u8 data[64];	/* buffer containing the command payload */
};

static struct dsi_panel_cmd nte300nts_init_cmds[] = {
	{0x01, 0, {0x00} },
	{REGFLAG_DELAY, 5, {} },

	{0xB0, 1, {0x00} },
	{REGFLAG_DELAY, 2, {} },

	{0xD6, 1, {0x01} },
	{REGFLAG_DELAY, 2, {} },

	{0xB3, 5, {0x14, 0x08, 0x00, 0x22, 0x00} },
	{REGFLAG_DELAY, 2, {} },

	{0xB4, 1, {0x0C} },
	{REGFLAG_DELAY, 2, {} },

	{0xB6, 2, {0x3A, 0xC3} },
	{REGFLAG_DELAY, 2, {} },

	{0x2A, 4, {0x00, 0x00, 0X04, 0XAF} },
	{REGFLAG_DELAY, 2, {} },

	{0x2B, 4, {0x00, 0x00, 0X07, 0X7F} },
	{REGFLAG_DELAY, 2, {} },

	{0x51, 1, {0xA6} },
	{REGFLAG_DELAY, 2, {} },

	{0x53, 1, {0x2C} },
	{REGFLAG_DELAY, 2, {} },

	{0x3A, 1, {0x66} },
	{REGFLAG_DELAY, 2, {} },

	{0x29, 0, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	{0x11, 0, {0x00} },
	{REGFLAG_DELAY, 150, {} },
};

static struct dsi_panel_cmd nte300nts_off_cmds[] = {
	{0x28, 0, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	{0x10, 0, {0x00} },
	{REGFLAG_DELAY, 80, {} },
};

static int hikey_panel_write_cmds(struct mipi_dsi_device *dsi,
				  struct dsi_panel_cmd *cmds,
				  u32 count)
{
	struct dsi_panel_cmd *cmd;
	int ret = 0;
	u32 i;

	for (i = 0; i < count; i++) {
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

static int hikey_panel_unprepare(struct drm_panel *p)
{
	struct hikey_panel *panel = to_hikey_panel(p);

	if (!panel->prepared)
		return 0;

	gpiod_set_value(panel->gpio_bl_en, 0);
	gpiod_set_value(panel->gpio_pwm, 0);

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
	ret = hikey_panel_write_cmds(panel->dsi, nte300nts_init_cmds,
				     ARRAY_SIZE(nte300nts_init_cmds));
	if (ret < 0)
		return ret;

	panel->prepared = true;

	return 0;
}

static int hikey_panel_disable(struct drm_panel *p)
{
	struct hikey_panel *panel = to_hikey_panel(p);
	int ret;

	if (!panel->enabled)
		return 0;

	ret = hikey_panel_write_cmds(panel->dsi, nte300nts_off_cmds,
				     ARRAY_SIZE(nte300nts_off_cmds));
	if (ret < 0)
		return ret;

	panel->enabled = false;

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
	.clock = 144000,

	.hdisplay = 1200,
	.hsync_start = 1200 + 200,
	.hsync_end = 1200 + 200 + 12,
	.htotal = 1200 + 12 + 60 + 200,

	.vdisplay = 1920,
	.vsync_start = 1920 + 8,
	.vsync_end = 1920 + 8 + 2,
	.vtotal = 1920 + 2 + 8 + 8,
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
	panel->connector->display_info.height_mm = 151;

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
	int ret = 0;

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

	panel->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(panel->vdd)) {
		ret = PTR_ERR(panel->vdd);
		return ret;
	}

	ret = regulator_set_voltage(panel->vdd, 1800000, 1800000);
	if (ret)
		return ret;

	ret = regulator_enable(panel->vdd);
	if (ret)
		return ret;

	return 0;
}

static int hikey_panel_attach_dsi(struct mipi_dsi_device *dsi)
{
	int ret;

	dsi->phy_clock = 864000; /* in kHz */
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
	if (ret) {
		hikey_panel_del(panel);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, panel);

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
	{ .compatible = "hisilicon,mipi-hikey", },
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

MODULE_DESCRIPTION("NTE300NTS (1920x1200) video mode panel driver");
MODULE_LICENSE("GPL v2");

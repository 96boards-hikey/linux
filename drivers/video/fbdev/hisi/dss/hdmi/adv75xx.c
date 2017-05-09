/**
 *
 *
 **/

#include "adv75xx.h"

#define HPD_ENABLE	0


static void adv75xx_power_on(struct adi_hdmi *adv75xx);
static void adv75xx_power_off(struct adi_hdmi *adv75xx);

/* ADI recommended values for proper operation. */
static const struct reg_sequence adv7511_fixed_registers[] = {
	{0x98, 0x03},
	{0x9a, 0xe0},
	{0x9c, 0x30},
	{0x9d, 0x61},
	{0xa2, 0xa4},
	{0xa3, 0xa4},
	{0xe0, 0xd0},
	{0xf9, 0x00},
	{0x55, 0x02},
};

/* ADI recommended values for proper operation. */
static const struct reg_sequence adv7533_fixed_registers[] = {
	{0x16, 0x20},
	{0x9a, 0xe0},
	{0xba, 0x70},
	{0xde, 0x82},
	{0xe4, 0x40},
	{0xe5, 0x80},
};

static const struct reg_sequence adv7533_cec_fixed_registers[] = {
	{0x15, 0xd0},
	{0x17, 0xd0},
	{0x24, 0x20},
	{0x57, 0x11},
	{0x05, 0xc8},
};

/* -----------------------------------------------------------------------------
 * Register access
 */

static const uint8_t adv75xx_register_defaults[] = {
	0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 00 */
	0x00, 0x00, 0x01, 0x0e, 0xbc, 0x18, 0x01, 0x13,
	0x25, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 10 */
	0x46, 0x62, 0x04, 0xa8, 0x00, 0x00, 0x1c, 0x84,
	0x1c, 0xbf, 0x04, 0xa8, 0x1e, 0x70, 0x02, 0x1e,	/* 20 */
	0x00, 0x00, 0x04, 0xa8, 0x08, 0x12, 0x1b, 0xac,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 30 */
	0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xb0,
	0x00, 0x50, 0x90, 0x7e, 0x79, 0x70, 0x00, 0x00,	/* 40 */
	0x00, 0xa8, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x0d, 0x00, 0x00, 0x00, 0x00,	/* 50 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 60 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 70 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00,	/* 90 */
	0x0b, 0x02, 0x00, 0x18, 0x5a, 0x60, 0x00, 0x00,
	0x00, 0x00, 0x80, 0x80, 0x08, 0x04, 0x00, 0x00,	/* a0 */
	0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x14,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* b0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* c0 */
	0x00, 0x03, 0x00, 0x00, 0x02, 0x00, 0x01, 0x04,
	0x30, 0xff, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00,	/* d0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x01,
	0x80, 0x75, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,	/* e0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x75, 0x11, 0x00,	/* f0 */
	0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static bool adv7511_register_volatile(struct device *dev, unsigned int reg)
{
	int ret = 0;

	switch (reg) {
	case ADV7533_REG_CHIP_REVISION:
	case ADV7533_REG_SPDIF_FREQ:
	case ADV7533_REG_CTS_AUTOMATIC1:
	case ADV7533_REG_CTS_AUTOMATIC2:
	case ADV7533_REG_VIC_DETECTED:
	case ADV7533_REG_VIC_SEND:
	case ADV7533_REG_AUX_VIC_DETECTED:
	case ADV7533_REG_STATUS:
	case ADV7533_REG_GC(1):
	case ADV7533_REG_INT(0):
	case ADV7533_REG_INT(1):
	case ADV7533_REG_PLL_STATUS:
	case ADV7533_REG_AN(0):
	case ADV7533_REG_AN(1):
	case ADV7533_REG_AN(2):
	case ADV7533_REG_AN(3):
	case ADV7533_REG_AN(4):
	case ADV7533_REG_AN(5):
	case ADV7533_REG_AN(6):
	case ADV7533_REG_AN(7):
	case ADV7533_REG_HDCP_STATUS:
	case ADV7533_REG_BCAPS:
	case ADV7533_REG_BKSV(0):
	case ADV7533_REG_BKSV(1):
	case ADV7533_REG_BKSV(2):
	case ADV7533_REG_BKSV(3):
	case ADV7533_REG_BKSV(4):
	case ADV7533_REG_DDC_STATUS:
	case ADV7533_REG_BSTATUS(0):
	case ADV7533_REG_BSTATUS(1):
	case ADV7533_REG_CHIP_ID_HIGH:
	case ADV7533_REG_CHIP_ID_LOW:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret ? true : false;
}

static const struct regmap_config adv75xx_regmap_config = {
	.name = "adv75xx",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults_raw = adv75xx_register_defaults,
	.num_reg_defaults_raw = ARRAY_SIZE(adv75xx_register_defaults),
	.volatile_reg = adv7511_register_volatile,
};

static const struct regmap_config adv7533_cec_regmap_config = {
	.name = "adv7533_cec",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.cache_type = REGCACHE_RBTREE,
};

static const struct regmap_config adv7533_packet_regmap_config = {
	.name = "adv7533_packet",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.cache_type = REGCACHE_RBTREE,
};

/* -----------------------------------------------------------------------------
 * Hardware configuration
 */
static void adv75xx_set_colormap(struct adi_hdmi *adv75xx, bool enable,
				 const u16 *coeff, unsigned int scaling_factor)
{
	unsigned int i;

	HISI_FB_INFO("+.\n");

	regmap_update_bits(adv75xx->regmap, ADV7533_REG_CSC_UPPER(1),
			   ADV7533_CSC_UPDATE_MODE, ADV7533_CSC_UPDATE_MODE);

	if (enable) {
		for (i = 0; i < 12; ++i) {
			regmap_update_bits(adv75xx->regmap,
					   ADV7533_REG_CSC_UPPER(i),
					   0x1f, coeff[i] >> 8);
			regmap_write(adv75xx->regmap,
				     ADV7533_REG_CSC_LOWER(i), coeff[i] & 0xff);
		}
	}

	if (enable)
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_CSC_UPPER(0),
				   0xe0, 0x80 | (scaling_factor << 5));
	else
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_CSC_UPPER(0),
				   0x80, 0x00);

	regmap_update_bits(adv75xx->regmap, ADV7533_REG_CSC_UPPER(1),
			   ADV7533_CSC_UPDATE_MODE, 0);

	HISI_FB_INFO("-.\n");
}

int adv75xx_packet_enable(struct adi_hdmi *adv75xx, unsigned int packet)
{
	HISI_FB_INFO("+.\n");

	if (packet & 0xff)
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_PACKET_ENABLE0,
				   packet, 0xff);

	if (packet & 0xff00) {
		packet >>= 8;
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_PACKET_ENABLE1,
				   packet, 0xff);
	}

	HISI_FB_INFO("-.\n");

	return 0;
}

int adv75xx_packet_disable(struct adi_hdmi *adv75xx, unsigned int packet)
{
	HISI_FB_INFO("+.\n");

	if (packet & 0xff)
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_PACKET_ENABLE0,
				   packet, 0x00);

	if (packet & 0xff00) {
		packet >>= 8;
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_PACKET_ENABLE1,
				   packet, 0x00);
	}

	HISI_FB_INFO("-.\n");

	return 0;
}

/* Coefficients for adv75xx color space conversion */
static const uint16_t adv75xx_csc_ycbcr_to_rgb[] = {
	0x0734, 0x04ad, 0x0000, 0x1c1b,
	0x1ddc, 0x04ad, 0x1f24, 0x0135,
	0x0000, 0x04ad, 0x087c, 0x1b77,
};

static void adv75xx_set_config_csc(struct adi_hdmi *adv75xx, bool rgb)
{
	struct adv75xx_video_config config;
	bool output_format_422, output_format_ycbcr;
	unsigned int mode;
	uint8_t infoframe[17];

	HISI_FB_INFO("+.\n");

	if (adv75xx->edid)
		config.hdmi_mode = true;
	else
		config.hdmi_mode = false;

	config.avi_infoframe.scan_mode = HDMI_SCAN_MODE_UNDERSCAN;

	HISI_FB_INFO("adv75xx->rgb is %d\n", adv75xx->rgb);

	if (rgb) {
		config.csc_enable = false;
		config.avi_infoframe.colorspace = HDMI_COLORSPACE_RGB;
	} else {
		config.csc_scaling_factor = ADV75xx_CSC_SCALING_4;
		config.csc_coefficents = adv75xx_csc_ycbcr_to_rgb;
	}

	HISI_FB_INFO("config.avi_infoframe.colorspace = %d\n",
		     config.avi_infoframe.colorspace);

	if (config.hdmi_mode) {
		mode = ADV7533_HDMI_CFG_MODE_HDMI;

		switch (config.avi_infoframe.colorspace) {
		case HDMI_COLORSPACE_YUV444:
			output_format_422 = false;
			output_format_ycbcr = true;
			break;
		case HDMI_COLORSPACE_YUV422:
			output_format_422 = true;
			output_format_ycbcr = true;
			break;
		default:
			output_format_422 = false;
			output_format_ycbcr = false;
			break;
		}
	} else {
		mode = ADV7533_HDMI_CFG_MODE_DVI;
		output_format_422 = false;
		output_format_ycbcr = false;
	}

	adv75xx_packet_disable(adv75xx, ADV7533_PACKET_ENABLE_AVI_INFOFRAME);

	adv75xx_set_colormap(adv75xx, config.csc_enable,
			     config.csc_coefficents, config.csc_scaling_factor);

	regmap_update_bits(adv75xx->regmap, ADV7533_REG_VIDEO_INPUT_CFG1, 0x81,
			   (output_format_422 << 7) | output_format_ycbcr);

	regmap_update_bits(adv75xx->regmap, ADV7533_REG_HDCP_HDMI_CFG,
			   ADV7533_HDMI_CFG_MODE_MASK, mode);

	/* The AVI infoframe id is not configurable */
	regmap_bulk_write(adv75xx->regmap, ADV7533_REG_AVI_INFOFRAME_VERSION,
			  infoframe + 1, sizeof(infoframe) - 1);

	adv75xx_packet_enable(adv75xx, ADV7533_PACKET_ENABLE_AVI_INFOFRAME);

	HISI_FB_INFO("-.\n");
}

static void adv75xx_dsi_config_tgen(struct adi_hdmi *adv75xx)
{
	u8 clock_div_by_lanes[] = { 6, 4, 3 };	/* 2, 3, 4 lanes */
	unsigned int hsw, hfp, hbp, vsw, vfp, vbp;

	HISI_FB_INFO("+.\n");

	hsw = adv75xx->mode->hsync_pulse_width;
	hfp = adv75xx->mode->hsync_offset;
	hbp = adv75xx->mode->htotal - adv75xx->mode->hsync_end;
	vsw = adv75xx->mode->vsync_pulse_width;
	vfp = adv75xx->mode->vsync_offset;
	vbp = adv75xx->mode->vtotal - adv75xx->mode->vsync_end;

	HISI_FB_INFO
	    ("hsw = %d, hfp = %d, hbp = %d, vsw = %d, vfp= %d, vbp = %d\n", hsw,
	     hfp, hbp, vsw, vfp, vbp);

#ifdef TEST_COLORBAR_DISPLAY
	/* set pixel clock auto mode */
	regmap_write(adv75xx->regmap_cec, ADV7533_REG_CEC_PIXEL_CLOCK_DIV,
		     0x00);
#else
	/* set pixel clock divider mode */
	regmap_write(adv75xx->regmap_cec, ADV7533_REG_CEC_PIXEL_CLOCK_DIV,
		     clock_div_by_lanes[adv75xx->num_dsi_lanes - 2] << 3);
#endif

	HISI_FB_INFO("dsi->lanes = %d, htotal = %d, vtotal = %d\n",
		     adv75xx->num_dsi_lanes, adv75xx->mode->htotal,
		     adv75xx->mode->vtotal);

	/* horizontal porch params */
	regmap_write(adv75xx->regmap_cec, 0x28, adv75xx->mode->htotal >> 4);
	regmap_write(adv75xx->regmap_cec, 0x29,
		     (adv75xx->mode->htotal << 4) & 0xff);
	regmap_write(adv75xx->regmap_cec, 0x2a, hsw >> 4);
	regmap_write(adv75xx->regmap_cec, 0x2b, (hsw << 4) & 0xff);
	regmap_write(adv75xx->regmap_cec, 0x2c, hfp >> 4);
	regmap_write(adv75xx->regmap_cec, 0x2d, (hfp << 4) & 0xff);
	regmap_write(adv75xx->regmap_cec, 0x2e, hbp >> 4);
	regmap_write(adv75xx->regmap_cec, 0x2f, (hbp << 4) & 0xff);

	/* vertical porch params */
	regmap_write(adv75xx->regmap_cec, 0x30, adv75xx->mode->vtotal >> 4);
	regmap_write(adv75xx->regmap_cec, 0x31,
		     (adv75xx->mode->vtotal << 4) & 0xff);
	regmap_write(adv75xx->regmap_cec, 0x32, vsw >> 4);
	regmap_write(adv75xx->regmap_cec, 0x33, (vsw << 4) & 0xff);
	regmap_write(adv75xx->regmap_cec, 0x34, vfp >> 4);
	regmap_write(adv75xx->regmap_cec, 0x35, (vfp << 4) & 0xff);
	regmap_write(adv75xx->regmap_cec, 0x36, vbp >> 4);
	regmap_write(adv75xx->regmap_cec, 0x37, (vbp << 4) & 0xff);

	/* 30Hz Low Refresh Rate (VIC Detection) */


	HISI_FB_INFO("-.\n");
}

static void adv75xx_dsi_receiver_dpms(struct adi_hdmi *adv75xx)
{
	HISI_FB_INFO("+.\n");

	if (adv75xx->type != ADV7533)
		return;

	if (adv75xx->powered) {
		adv75xx_dsi_config_tgen(adv75xx);

		/* set number of dsi lanes */
		regmap_write(adv75xx->regmap_cec, ADV7533_REG_DSI_DATA_LANES,
			     adv75xx->num_dsi_lanes << 4);

#ifdef TEST_COLORBAR_DISPLAY
		/* reset internal timing generator */
		regmap_write(adv75xx->regmap_cec, 0x27, 0xcb);
		regmap_write(adv75xx->regmap_cec, 0x27, 0x8b);
		regmap_write(adv75xx->regmap_cec, 0x27, 0xcb);
#else
		/* disable internal timing generator */
		regmap_write(adv75xx->regmap_cec, 0x27, 0x0b);
#endif

		/* 09-03 AVI Infoframe - RGB - 16-9 Aspect Ratio */
		regmap_write(adv75xx->regmap, 0x55, 0x10);
		regmap_write(adv75xx->regmap, 0x56, 0x28);

		/* 04-04 GC Packet Enable */
		regmap_write(adv75xx->regmap, 0x40, 0x80);

		/* 04-06 GC Colour Depth - 24 Bit */
		regmap_write(adv75xx->regmap, 0x4c, 0x04);

		/* 04-09 Down Dither Output Colour Depth - 8 Bit (default) */
		regmap_write(adv75xx->regmap, 0x49, 0x00);

		/* 07-01 CEC Power Mode - Always Active */
		regmap_write(adv75xx->regmap_cec, 0xbe, 0x3d);

		/* enable hdmi */
		regmap_write(adv75xx->regmap_cec, 0x03, 0x89);

#ifdef TEST_COLORBAR_DISPLAY
		/*enable test mode */
		regmap_write(adv75xx->regmap_cec, 0x55, 0x80);
#else
		/* disable test mode */
		regmap_write(adv75xx->regmap_cec, 0x55, 0x00);
#endif
		/* SPD */
		{
			static const unsigned char spd_if[] = {
				0x83, 0x01, 25, 0x00,
				'L', 'i', 'n', 'a', 'r', 'o', 0, 0,
				'9', '6', 'b', 'o', 'a', 'r', 'd', 's',
				':', 'H', 'i', 'k', 'e', 'y', 0, 0,
			};
			int n;

			for (n = 0; n < sizeof(spd_if); n++)
				regmap_write(adv75xx->regmap_packet, n,
					     spd_if[n]);

			/* enable send SPD */
			regmap_update_bits(adv75xx->regmap, 0x40, BIT(6),
					   BIT(6));
		}

		/* force audio */
		/* hide Audio infoframe updates */
		regmap_update_bits(adv75xx->regmap, 0x4a, BIT(5), BIT(5));

		/* i2s, internal mclk, mclk-256 */
		regmap_update_bits(adv75xx->regmap, 0x0a, 0x1f, 1);
		regmap_update_bits(adv75xx->regmap, 0x0b, 0xe0, 0);
		/* enable i2s, use i2s format, sample rate from i2s */
		regmap_update_bits(adv75xx->regmap, 0x0c, 0xc7, BIT(2));
		/* 16 bit audio */
		regmap_update_bits(adv75xx->regmap, 0x0d, 0xff, 16);
		/* 16-bit audio */
		regmap_update_bits(adv75xx->regmap, 0x14, 0x0f, 2 << 4);
		/* 48kHz */
		regmap_update_bits(adv75xx->regmap, 0x15, 0xf0, 2 << 4);
		/* enable N/CTS, enable Audio sample packets */
		regmap_update_bits(adv75xx->regmap, 0x44, BIT(5), BIT(5));
		/* N = 6144 */
		regmap_write(adv75xx->regmap, 1, (6144 >> 16) & 0xf);
		regmap_write(adv75xx->regmap, 2, (6144 >> 8) & 0xff);
		regmap_write(adv75xx->regmap, 3, (6144) & 0xff);
		/* automatic cts */
		regmap_update_bits(adv75xx->regmap, 0x0a, BIT(7), 0);
		/* enable N/CTS */
		regmap_update_bits(adv75xx->regmap, 0x44, BIT(6), BIT(6));
		/* not copyrighted */
		regmap_update_bits(adv75xx->regmap, 0x12, BIT(5), BIT(5));

		/* left source */
		regmap_update_bits(adv75xx->regmap, 0x0e, 7 << 3, 0);
		/* right source */
		regmap_update_bits(adv75xx->regmap, 0x0e, 7 << 0, 1);
		/* number of channels: sect 4.5.4: set to 0 */
		regmap_update_bits(adv75xx->regmap, 0x73, 7, 1);
		/* number of channels: sect 4.5.4: set to 0 */
		regmap_update_bits(adv75xx->regmap, 0x73, 0xf0, 1 << 4);
		/* sample rate: 48kHz */
		regmap_update_bits(adv75xx->regmap, 0x74, 7 << 2, 3 << 2);
		/* channel allocation reg: sect 4.5.4: set to 0 */
		regmap_update_bits(adv75xx->regmap, 0x76, 0xff, 0);
		/* enable audio infoframes */
		regmap_update_bits(adv75xx->regmap, 0x44, BIT(3), BIT(3));

		/* AV mute disable */
		regmap_update_bits(adv75xx->regmap, 0x4b, BIT(7) | BIT(6),
				   BIT(7));

		/* use Audio infoframe updated info */
		regmap_update_bits(adv75xx->regmap, 0x4a, BIT(5), 0);
	} else {
		regmap_write(adv75xx->regmap_cec, 0x03, 0x0b);
		regmap_write(adv75xx->regmap_cec, 0x27, 0x0b);
	}

	HISI_FB_INFO("-.\n");
}

/* -----------------------------------------------------------------------------
 * Interrupt and hotplug detection
 */

#if HPD_ENABLE
static bool adv75xx_hpd(struct adi_hdmi *adv75xx)
{
	unsigned int irq0;
	int ret;

	HISI_FB_INFO("+.\n");

	ret = regmap_read(adv75xx->regmap, ADV7533_REG_INT(0), &irq0);
	if (ret < 0)
		return false;

	HISI_FB_INFO("irq0 = 0x%x\n", irq0);

	if (irq0 & ADV7533_INT0_HDP) {
		HISI_FB_INFO("HPD interrupt detected!\n");
		regmap_write(adv75xx->regmap, ADV7533_REG_INT(0),
			     ADV7533_INT0_HDP);
		return true;
	}

	HISI_FB_INFO("-.\n");

	return false;
}
#endif

static int adv75xx_irq_process(struct adi_hdmi *adv75xx, bool process_hpd)
{
	unsigned int irq0, irq1;
	int ret;

	HISI_FB_INFO("+.\n");

	ret = regmap_read(adv75xx->regmap, ADV7533_REG_INT(0), &irq0);
	if (ret < 0)
		return ret;

	ret = regmap_read(adv75xx->regmap, ADV7533_REG_INT(1), &irq1);
	if (ret < 0)
		return ret;

	regmap_write(adv75xx->regmap, ADV7533_REG_INT(0), irq0);
	regmap_write(adv75xx->regmap, ADV7533_REG_INT(1), irq1);

	HISI_FB_INFO("adv7511_irq_process --> irq0 = 0x%x \n", irq0);
	HISI_FB_INFO("adv7511_irq_process --> irq1 = 0x%x \n", irq1);

	if (irq0 & ADV7533_INT0_EDID_READY || irq1 & ADV7533_INT1_DDC_ERROR) {
		adv75xx->edid_read = true;
		if (adv75xx->i2c_main->irq)
			HISI_FB_INFO("adv7511_irq_process -->get i2c_main irq \n");

		wake_up_all(&adv75xx->wq);
	}

	HISI_FB_INFO("-.\n");

	return 0;
}

static irqreturn_t adv75xx_irq_handler(int irq, void *devid)
{
	struct adi_hdmi *adv75xx = devid;
	int ret;

	HISI_FB_INFO("+.\n");

	ret = adv75xx_irq_process(adv75xx, true);

	HISI_FB_INFO("-.\n");

	return ret < 0 ? IRQ_NONE : IRQ_HANDLED;
}

/* -----------------------------------------------------------------------------
 * EDID retrieval
 */

static int adv75xx_wait_for_edid(struct adi_hdmi *adv75xx, int timeout)
{
	int ret;

	HISI_FB_INFO("+.\n");

	if (adv75xx->i2c_main->irq) {
		ret = wait_event_interruptible_timeout(adv75xx->wq,
						       adv75xx->edid_read,
						       msecs_to_jiffies
						       (timeout));
	} else {
		for (; timeout > 0; timeout -= 25) {
			ret = adv75xx_irq_process(adv75xx, false);
			if (ret < 0)
				break;

			if (adv75xx->edid_read)
				break;

			msleep(25);
		}
	}

	HISI_FB_INFO("-.\n");

	return adv75xx->edid_read ? 0 : -EIO;
}

static void print_edid_info(u8 *block)
{
	int step, count;

	count = 0x0;
	while (count < EDID_LENGTH) {
		step = 0;
		do {
			if (step == 0) {
				HISI_FB_INFO("------ edid[%d]: 0x%2x \t", count,
					     block[count]);
			} else {
				HISI_FB_INFO(" 0x%2x \t", block[count]);
			}
			step++;
			count++;
		} while (step < 8);

		HISI_FB_INFO("\n");
	}
}

struct hisi_display_mode *hisi_set_mode_info(void)
{
	struct hisi_display_mode *mode;
	unsigned int hsw, hfp, hbp, vsw, vfp, vbp;

	mode = kzalloc(sizeof(struct hisi_display_mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->width_mm = 160 * 100;
	mode->height_mm = 90 * 100;
	mode->clock = 148500;
	mode->hdisplay = 1920;
	mode->vdisplay = 1080;
	mode->hsync_offset = 88;
	mode->hsync_pulse_width = 44;
	mode->hsync_start = 2008;
	mode->hsync_end = 2052;
	mode->htotal = 2200;

	mode->vsync_offset = 4;
	mode->vsync_pulse_width = 5;
	mode->vsync_start = 1084;
	mode->vsync_end = 1089;
	mode->vtotal = 1125;

	hsw = mode->hsync_pulse_width;
	hfp = mode->hsync_offset;
	hbp = mode->htotal - mode->hsync_end;
	vsw = mode->vsync_pulse_width;
	vfp = mode->vsync_offset;
	vbp = mode->vtotal - mode->vsync_end;

	HISI_FB_INFO("The pixel clock is %d!!\n", mode->clock);
	HISI_FB_INFO("The resolution is %d x %d !!\n", mode->hdisplay,
		     mode->vdisplay);
	HISI_FB_INFO
	    ("hsw = %d, hfp = %d, hbp = %d, vsw = %d, vfp= %d, vbp = %d\n", hsw,
	     hfp, hbp, vsw, vfp, vbp);


	return mode;
}

struct hisi_display_mode *hisi_parse_edid_base_info(u8 *block)
{
	struct hisi_display_mode *mode;
	char edid_vendor[3];
	unsigned hblank;
	unsigned vblank;
	unsigned int hsw, hfp, hbp, vsw, vfp, vbp;

	mode = kzalloc(sizeof(struct hisi_display_mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	edid_vendor[0] = ((block[8] & 0x7c) >> 2) + '@';
	edid_vendor[1] = (((block[8] & 0x3) << 3) |
			  ((block[1] & 0xe0) >> 5)) + '@';
	edid_vendor[2] = (block[9] & 0x1f) + '@';

	mode->width_mm = block[21] * 100;
	mode->height_mm = block[22] * 100;
	HISI_FB_INFO("The product vender is %c%c%c !!!\n", edid_vendor[0],
		     edid_vendor[1], edid_vendor[2]);
	HISI_FB_INFO
	    ("The screen supported max width is %d cm, max height is %d cm !!\n",
	     block[21], block[22]);
	HISI_FB_INFO("The display gamma is %d !!\n", block[23]);
	HISI_FB_INFO("The display is RGB or YCbCr is 0x%x !!\n",
		     (block[24] & 0x18) >> 3);
	/******** Detailed Timing Descriptor **********/
	mode->clock = (block[55] << 8 | block[54]) * 10;
	mode->hdisplay = ((block[58] & 0xf0) << 4) | block[56];
	hblank = ((block[58] & 0x0f) << 8) | block[57];
	mode->vdisplay = ((block[61] & 0xf0) << 4) | block[59];
	vblank = ((block[61] & 0x0f) << 8) | block[60];
	mode->hsync_offset = block[62];
	mode->hsync_pulse_width = block[63];
	mode->vsync_offset = (block[64] & 0xf0) >> 4;
	mode->vsync_pulse_width = block[64] & 0x0f;

	mode->hsync_start = mode->hdisplay + mode->hsync_offset;
	mode->hsync_end = mode->hsync_start + mode->hsync_pulse_width;
	mode->htotal = mode->hdisplay + hblank;
	mode->vsync_start = mode->vdisplay + mode->vsync_offset;
	mode->vsync_end = mode->vsync_start + mode->vsync_pulse_width;
	mode->vtotal = mode->vdisplay + vblank;

	hsw = mode->hsync_pulse_width;
	hfp = mode->hsync_offset;
	hbp = mode->htotal - mode->hsync_end;
	vsw = mode->vsync_pulse_width;
	vfp = mode->vsync_offset;
	vbp = mode->vtotal - mode->vsync_end;

	HISI_FB_INFO("The pixel clock is %d!!\n", mode->clock);
	HISI_FB_INFO("The resolution is %d x %d !!\n", mode->hdisplay,
		     mode->vdisplay);
	HISI_FB_INFO
	    ("hsw = %d, hfp = %d, hbp = %d, vsw = %d, vfp= %d, vbp = %d\n", hsw,
	     hfp, hbp, vsw, vfp, vbp);


	return mode;
}

static int adv75xx_get_edid_block(void *data, u8 *buf, unsigned int block,
				  size_t len)
{
	struct adi_hdmi *adv75xx = data;
	struct i2c_msg xfer[2];
	uint8_t offset, edid_buf[256];
	unsigned int i;
	int ret;

	if (len > EDID_LENGTH)
		return -EINVAL;

	HISI_FB_INFO("+.\n");

	if (adv75xx->current_edid_segment != block) {
		unsigned int status;

		ret = regmap_read(adv75xx->regmap, ADV7533_REG_DDC_STATUS,
				  &status);
		if (ret < 0)
			return ret;

		if (status != IDLE) {
			adv75xx->edid_read = false;
			regmap_write(adv75xx->regmap, ADV7533_REG_EDID_SEGMENT,
				     block);
			ret = adv75xx_wait_for_edid(adv75xx, 200);
			if (ret < 0)
				return ret;
		}

		/* Break this apart, hopefully more I2C controllers will
		 * support 64 byte transfers than 256 byte transfers
		 */

		xfer[0].addr = adv75xx->i2c_edid->addr;
		xfer[0].flags = 0;
		xfer[0].len = 1;
		xfer[0].buf = &offset;
		xfer[1].addr = adv75xx->i2c_edid->addr;
		xfer[1].flags = I2C_M_RD;
		xfer[1].len = 64;
		xfer[1].buf = edid_buf;

		offset = 0;

		for (i = 0; i < 4; ++i) {
			ret = i2c_transfer(adv75xx->i2c_edid->adapter, xfer,
					   ARRAY_SIZE(xfer));
			if (ret < 0)
				return ret;
			else if (ret != 2)
				return -EIO;

			xfer[1].buf += 64;
			offset += 64;
		}

		adv75xx->current_edid_segment = block;
	}

	if (block % 2 == 0)
		memcpy(buf, edid_buf, len);
	else
		memcpy(buf, edid_buf + EDID_LENGTH, len);

	HISI_FB_INFO("-.\n");

	return 0;
}

static bool edid_is_zero(const u8 *in_edid, int length)
{
	if (memchr_inv(in_edid, 0, length))
		return false;

	return true;
}

/**
 * hisi_do_get_edid - get EDID data using a custom EDID block read function
 * @get_edid_block: EDID block read function
 * @data: private data passed to the block read function
 *
 * When the I2C adapter connected to the DDC bus is hidden behind a device that
 * exposes a different interface to read EDID blocks this function can be used
 * to get EDID data using a custom block read function.
 *
 * As in the general case the DDC bus is accessible by the kernel at the I2C
 * level, drivers must make all reasonable efforts to expose it as an I2C
 * adapter and use drm_get_edid() instead of abusing this function.
 *
 * Return: Pointer to valid EDID or NULL if we couldn't find any.
 */
struct edid *hisi_do_get_edid(int (*get_edid_block) (void *data, u8 *buf,
						     unsigned int block,
						     size_t len), void *data)
{
	u8 *block;
	bool print_bad_edid = true;

	HISI_FB_INFO("+.\n");

	if ((block = kmalloc(EDID_LENGTH, GFP_KERNEL)) == NULL)
		return NULL;

	HISI_FB_INFO("EDID_LENGTH = %d \n", EDID_LENGTH);
	/* base block fetch */
	if (get_edid_block(data, block, 0, EDID_LENGTH))
		goto out;

	if (edid_is_zero(block, EDID_LENGTH))
		goto carp;

	HISI_FB_INFO("edid_block read success!!!\n");

	print_edid_info(block);

	return (struct edid *)block;

 carp:
	if (print_bad_edid)
		HISI_FB_ERR("EDID block invalid.\n");
 out:
	kfree(block);
	return NULL;
}

struct hisi_display_mode *adv75xx_get_modes(struct adi_hdmi *adv75xx)
{
	struct edid *edid;
	struct hisi_display_mode *mode;

	HISI_FB_INFO("+.\n");

	/* Reading the EDID only works if the device is powered */
	if (!adv75xx->powered) {
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_POWER2, ADV7533_REG_POWER2_HDP_SRC_MASK, ADV7533_REG_POWER2_HDP_SRC_NONE);	/* 0xc0 */
		regmap_write(adv75xx->regmap, ADV7533_REG_INT(0),
			     ADV7533_INT0_EDID_READY);
		regmap_write(adv75xx->regmap, ADV7533_REG_INT(1),
			     ADV7533_INT1_DDC_ERROR);
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_POWER, ADV7533_POWER_POWER_DOWN, 0);	/* 0x41 0x10 */
		adv75xx->current_edid_segment = -1;
		/* wait some time for edid is ready */
		msleep(200);
	}

	edid = hisi_do_get_edid(adv75xx_get_edid_block, adv75xx);

	if (!adv75xx->powered)
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_POWER,
				   ADV7533_POWER_POWER_DOWN,
				   ADV7533_POWER_POWER_DOWN);

	kfree(adv75xx->edid);
	adv75xx->edid = edid;
	if (!edid) {
		HISI_FB_ERR("Fail to get really edid info !!!\n");
		mode = hisi_set_mode_info();
		return mode;
	}

	mode = hisi_parse_edid_base_info((u8 *) adv75xx->edid);

	adv75xx_set_config_csc(adv75xx, adv75xx->rgb);

	HISI_FB_INFO("-.\n");

	return mode;
}

/*==========================================================*/
static enum connector_status adv75xx_detect(struct adi_hdmi *adv75xx)
{
	enum connector_status status;
	unsigned int val;
#if HPD_ENABLE
	bool hpd;
#endif
	int ret;

	HISI_FB_INFO("+.\n");

	ret = regmap_read(adv75xx->regmap, ADV7533_REG_STATUS, &val);
	if (ret < 0) {
		HISI_FB_INFO("HDMI connector status is disconnected !!!\n");
		return connector_status_disconnected;
	}

	if (val & ADV7533_STATUS_HPD)
		status = connector_status_connected;
	else
		status = connector_status_disconnected;

#if HPD_ENABLE
	hpd = adv75xx_hpd(adv75xx);

	/* The chip resets itself when the cable is disconnected, so in case
	 * there is a pending HPD interrupt and the cable is connected there was
	 * at least one transition from disconnected to connected and the chip
	 * has to be reinitialized. */
	if (status == connector_status_connected && hpd && adv75xx->powered) {
		regcache_mark_dirty(adv75xx->regmap);
		adv75xx_power_on(adv75xx);
		adv75xx_get_modes(adv75xx);
		if (adv75xx->status == connector_status_connected)
			status = connector_status_disconnected;
	} else {
		/* Renable HDP sensing */
		regmap_update_bits(adv75xx->regmap, ADV7533_REG_POWER2,
				   ADV7533_REG_POWER2_HDP_SRC_MASK,
				   ADV7533_REG_POWER2_HDP_SRC_BOTH);
	}
#endif

	adv75xx->status = status;

	HISI_FB_INFO("adv7511->status = %d <1-connected,2-disconnected>\n",
		     status);
	HISI_FB_INFO("-.\n");

	return status;
}

/**
 * mode_vrefresh - get the vrefresh of a mode
 * @mode: mode
 *
 * Returns:
 * @modes's vrefresh rate in Hz, rounded to the nearest integer. Calculates the
 * value first if it is not yet set.
 */
static int mode_vrefresh(const struct hisi_display_mode *mode)
{
	int refresh = 0;
	unsigned int calc_val;

	if (mode->vrefresh > 0)
		refresh = mode->vrefresh;
	else if (mode->htotal > 0 && mode->vtotal > 0) {
		int vtotal;
		vtotal = mode->vtotal;
		/* work out vrefresh the value will be x1000 */
		calc_val = (mode->clock * 1000);
		calc_val /= mode->htotal;
		refresh = (calc_val + vtotal / 2) / vtotal;
	}
	return refresh;
}

static int adv75xx_mode_valid(struct hisi_display_mode *mode)
{
	if (NULL == mode) {
		HISI_FB_ERR("mode is null\n");
		return MODE_NOMODE;
	}

	if (mode->clock > 165000)
		return MODE_CLOCK_HIGH;
	/*
	 * some work well modes which want to put in the front of the mode list.
	 */
	HISI_FB_INFO("Checking mode %ix%i@%i clock: %i...",
		     mode->hdisplay, mode->vdisplay, mode_vrefresh(mode),
		     mode->clock);
	if ((mode->hdisplay == 1920 && mode->vdisplay == 1080
	     && mode->clock == 148500) || (mode->hdisplay == 1280
					   && mode->vdisplay == 800
					   && mode->clock == 83496)
	    || (mode->hdisplay == 1280 && mode->vdisplay == 720
		&& mode->clock == 74440) || (mode->hdisplay == 1280
					     && mode->vdisplay == 720
					     && mode->clock == 74250)
	    || (mode->hdisplay == 1024 && mode->vdisplay == 768
		&& mode->clock == 75000) || (mode->hdisplay == 1024
					     && mode->vdisplay == 768
					     && mode->clock == 81833)
	    || (mode->hdisplay == 800 && mode->vdisplay == 600
		&& mode->clock == 40000)) {
		HISI_FB_INFO("OK\n");
		return MODE_OK;
	}
	HISI_FB_INFO("BAD\n");

	return MODE_BAD;
}

static void adv75xx_mode_set(struct adi_hdmi *adv75xx,
			     struct hisi_display_mode *mode)
{
	unsigned int low_refresh_rate;
	unsigned int hsync_polarity = 0;
	unsigned int vsync_polarity = 0;

	HISI_FB_INFO("+.\n");

	if (adv75xx->embedded_sync) {
		unsigned int hsync_offset, hsync_len;
		unsigned int vsync_offset, vsync_len;

		hsync_offset = mode->hsync_offset;
		vsync_offset = mode->vsync_offset;
		hsync_len = mode->hsync_end - mode->hsync_start;
		vsync_len = mode->vsync_end - mode->vsync_start;

		/* The hardware vsync generator has a off-by-one bug */
		vsync_offset += 1;

		regmap_write(adv75xx->regmap, ADV7533_REG_HSYNC_PLACEMENT_MSB,
			     ((hsync_offset >> 10) & 0x7) << 5);
		regmap_write(adv75xx->regmap, ADV7533_REG_SYNC_DECODER(0),
			     (hsync_offset >> 2) & 0xff);
		regmap_write(adv75xx->regmap, ADV7533_REG_SYNC_DECODER(1),
			     ((hsync_offset & 0x3) << 6) |
			     ((hsync_len >> 4) & 0x3f));
		regmap_write(adv75xx->regmap, ADV7533_REG_SYNC_DECODER(2),
			     ((hsync_len & 0xf) << 4) |
			     ((vsync_offset >> 6) & 0xf));
		regmap_write(adv75xx->regmap, ADV7533_REG_SYNC_DECODER(3),
			     ((vsync_offset & 0x3f) << 2) |
			     ((vsync_len >> 8) & 0x3));
		regmap_write(adv75xx->regmap, ADV7533_REG_SYNC_DECODER(4),
			     vsync_len & 0xff);

		hsync_polarity = !(mode->flags & MODE_FLAG_PHSYNC);
		vsync_polarity = !(mode->flags & MODE_FLAG_PVSYNC);
	} else {
		/**
		 * If the input signal is always low or always high we want to
		 * invert or let it passthrough depending on the polarity of the
		 * current mode.
		 **/
		adv75xx->hsync_polarity = ADV7533_SYNC_POLARITY_PASSTHROUGH;
		adv75xx->vsync_polarity = ADV7533_SYNC_POLARITY_PASSTHROUGH;

		hsync_polarity = adv75xx->hsync_polarity;
		vsync_polarity = adv75xx->vsync_polarity;
	}
	mode->vrefresh = mode_vrefresh(mode);
	HISI_FB_INFO("hsync_polarity = %d; vsync_polarity = %d\n",
		     hsync_polarity, vsync_polarity);
	HISI_FB_INFO("mode->vrefresh = %d \n", mode->vrefresh);

	if (mode->vrefresh <= 24000)
		low_refresh_rate = ADV7533_LOW_REFRESH_RATE_24HZ;
	else if (mode->vrefresh <= 25000)
		low_refresh_rate = ADV7533_LOW_REFRESH_RATE_25HZ;
	else if (mode->vrefresh <= 30000)
		low_refresh_rate = ADV7533_LOW_REFRESH_RATE_30HZ;
	else
		low_refresh_rate = ADV7533_LOW_REFRESH_RATE_NONE;

	HISI_FB_INFO("low_refresh_rate = %d \n", low_refresh_rate);

	regmap_update_bits(adv75xx->regmap, 0xfb, 0x6, low_refresh_rate << 1);
	regmap_update_bits(adv75xx->regmap, ADV7533_REG_SYNC_POLARITY,
			   0x60, (vsync_polarity << 6) | (hsync_polarity << 5));

	/*
	 * TODO Test first order 4:2:2 to 4:4:4 up conversion method, which is
	 * supposed to give better results.
	 */

	adv75xx->f_tmds = mode->clock;

	HISI_FB_INFO("-.\n");
}

static void adv75xx_power_on(struct adi_hdmi *adv75xx)
{
	HISI_FB_INFO("+.\n");

	adv75xx->current_edid_segment = -1;

	regmap_write(adv75xx->regmap, ADV7533_REG_INT(0),
		     ADV7533_INT0_EDID_READY);
	regmap_write(adv75xx->regmap, ADV7533_REG_INT(1),
		     ADV7533_INT1_DDC_ERROR);
	regmap_update_bits(adv75xx->regmap, ADV7533_REG_POWER,
			   ADV7533_POWER_POWER_DOWN, 0);

	/*
	 * Per spec it is allowed to pulse the HDP signal to indicate that the
	 * EDID information has changed. Some monitors do this when they wakeup
	 * from standby or are enabled. When the HDP goes low the adv7511 is
	 * reset and the outputs are disabled which might cause the monitor to
	 * go to standby again. To avoid this we ignore the HDP pin for the
	 * first few seconds after enabling the output.
	 */
	regmap_update_bits(adv75xx->regmap, ADV7533_REG_POWER2,
			   ADV7533_REG_POWER2_HDP_SRC_MASK,
			   ADV7533_REG_POWER2_HDP_SRC_NONE);

	/*
	 * Most of the registers are reset during power down or when HPD is low.
	 */
	regcache_sync(adv75xx->regmap);

	regmap_register_patch(adv75xx->regmap_cec,
			      adv7533_cec_fixed_registers,
			      ARRAY_SIZE(adv7533_cec_fixed_registers));
	adv75xx->powered = true;

	adv75xx_dsi_receiver_dpms(adv75xx);

	HISI_FB_INFO("-.\n");
}

static void adv75xx_power_off(struct adi_hdmi *adv75xx)
{
	HISI_FB_INFO("+.\n");

	/* TODO: setup additional power down modes */
	regmap_update_bits(adv75xx->regmap, ADV7533_REG_POWER,
			   ADV7533_POWER_POWER_DOWN, ADV7533_POWER_POWER_DOWN);
	regcache_mark_dirty(adv75xx->regmap);

	adv75xx->powered = false;

	adv75xx_dsi_receiver_dpms(adv75xx);

	HISI_FB_INFO("-.\n");
}

/* =========================================================*/
static int adv7533_init_regulators(struct adi_hdmi *adv75xx)
{
	int ret;
	struct device *dev = &adv75xx->i2c_main->dev;

	adv75xx->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(adv75xx->vdd)) {
		ret = PTR_ERR(adv75xx->vdd);
		dev_err(dev, "failed to get vdd regulator %d\n", ret);
		return ret;
	}

	adv75xx->v1p2 = devm_regulator_get(dev, "v1p2");
	if (IS_ERR(adv75xx->v1p2)) {
		ret = PTR_ERR(adv75xx->v1p2);
		dev_err(dev, "failed to get v1p2 regulator %d\n", ret);
		return ret;
	}

	ret = regulator_set_voltage(adv75xx->vdd, 1800000, 1800000);
	if (ret) {
		dev_err(dev, "failed to set avdd voltage %d\n", ret);
		return ret;
	}

	ret = regulator_set_voltage(adv75xx->v1p2, 1200000, 1200000);
	if (ret) {
		dev_err(dev, "failed to set v1p2 voltage %d\n", ret);
		return ret;
	}

	/* keep the regulators always on */
	ret = regulator_enable(adv75xx->vdd);
	if (ret) {
		dev_err(dev, "failed to enable vdd %d\n", ret);
		return ret;
	}

	ret = regulator_enable(adv75xx->v1p2);
	if (ret) {
		dev_err(dev, "failed to enable v1p2 %d\n", ret);
		return ret;
	}

	return 0;
}

static int adv7533_parse_dt(struct device_node *np, struct adi_hdmi *adv75xx)
{
	int ret;
	u32 num_lanes;

	ret = of_property_read_u32(np, "adi,dsi-lanes", &num_lanes);
	if (ret) {
		HISI_FB_WARNING("get 'adi,dsi-lanes' resource failed!\n");
		return ret;
	}

	if (num_lanes < 1 || num_lanes > 4)
		return -EINVAL;

	adv75xx->num_dsi_lanes = num_lanes;

	/* TODO: Check if these need to be parsed by DT or not */
	adv75xx->rgb = true;
	adv75xx->embedded_sync = false;

	return 0;
}

static const int edid_i2c_addr = 0x7e;
static const int packet_i2c_addr = 0x70;
static const int cec_i2c_addr = 0x78;

static const struct of_device_id adv75xx_of_ids[] = {
	{.compatible = "adi,adv7511", .data = (void *)ADV7511},
	{.compatible = "adi,adv7511w", .data = (void *)ADV7511},
	{.compatible = "adi,adv7513", .data = (void *)ADV7511},
	{.compatible = "adi,adv7533", .data = (void *)ADV7533},
	{},
};

MODULE_DEVICE_TABLE(of, adv75xx_of_ids);

static const struct i2c_device_id adv75xx_i2c_ids[] = {
	{"adv7511", ADV7511},
	{"adv7511w", ADV7511},
	{"adv7513", ADV7511},
	{"adv7533", ADV7533},
	{}
};

MODULE_DEVICE_TABLE(i2c, adv75xx_i2c_ids);

static struct adi_operation_funcs opt_funcs = {
	.power_on = adv75xx_power_on,
	.power_off = adv75xx_power_off,
	.mode_set = adv75xx_mode_set,
};

static int adv75xx_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct adi_hdmi *adv75xx;
	struct device *dev = &i2c->dev;
	enum connector_status status;
	struct hisi_display_mode *mode;
	struct platform_device *hdmi_pdev = NULL;
	unsigned int val;
	int ret;

	if (!dev) {
		HISI_FB_ERR("dev is  NULL!\n");
		return -ENOMEM;
	}

	adv75xx = devm_kzalloc(dev, sizeof(struct adi_hdmi), GFP_KERNEL);
	if (!adv75xx) {
		HISI_FB_ERR("adv75xx alloc  failed!\n");
		return -ENOMEM;
	}
	adv75xx->powered = false;
	adv75xx->status = connector_status_disconnected;

	if (dev->of_node) {
		const struct of_device_id *of_id;

		of_id = of_match_node(adv75xx_of_ids, dev->of_node);
		adv75xx->type = (enum adv75xx_type)of_id->data;
	} else {
		adv75xx->type = ADV7533;
	}

	ret = adv7533_parse_dt(dev->of_node, adv75xx);
	if (ret) {
		HISI_FB_ERR("parse dts error!\n");
		goto err_return;
	}

	adv75xx->i2c_main = i2c;

	if (adv75xx->type == ADV7533) {
		ret = adv7533_init_regulators(adv75xx);	/* adv7533 vdd--1.8v  v1p2--1.2v */
		if (ret)
			return ret;
	}

	/*
	 * The power down GPIO is optional. If present, toggle it from active(1) to
	 * inactive(0) to wake up the encoder.
	 */
	adv75xx->gpio_pd = devm_gpiod_get_optional(dev, "pd", GPIOD_OUT_HIGH);
	if (IS_ERR(adv75xx->gpio_pd)) {
		HISI_FB_ERR("get gpio pd error!\n");
		return PTR_ERR(adv75xx->gpio_pd);
	}
	HISI_FB_INFO("adv75xx->gpio_pd = %s!\n", adv75xx->gpio_pd->label);

	if (adv75xx->gpio_pd) {
		mdelay(5);
		gpiod_set_value_cansleep(adv75xx->gpio_pd, 0);
	}

	adv75xx->regmap = devm_regmap_init_i2c(i2c, &adv75xx_regmap_config);
	if (IS_ERR(adv75xx->regmap)) {
		HISI_FB_ERR("regmap init i2c failed!\n");
		return PTR_ERR(adv75xx->regmap);
	}

	ret = regmap_read(adv75xx->regmap, ADV7533_REG_CHIP_REVISION, &val);
	if (ret) {
		HISI_FB_ERR("regmap read failed, ret = %d!\n", ret);
		goto err_return;
	}
	HISI_FB_INFO("%s of the Chip reversion is %d\n", dev_name(dev), val);	/* the corect val is 20. */
	dev_err(dev, "Rev. %d\n", val);

	ret = regmap_register_patch(adv75xx->regmap,
				    adv7533_fixed_registers,
				    ARRAY_SIZE(adv7533_fixed_registers));
	if (ret) {
		HISI_FB_ERR("regmap register failed!\n");
		goto err_return;
	}

	regmap_write(adv75xx->regmap, ADV7533_REG_EDID_I2C_ADDR, edid_i2c_addr);
	regmap_write(adv75xx->regmap, ADV7533_REG_PACKET_I2C_ADDR,
		     packet_i2c_addr);
	regmap_write(adv75xx->regmap, ADV7533_REG_CEC_I2C_ADDR, cec_i2c_addr);
	adv75xx_packet_disable(adv75xx, 0xffff);

	adv75xx->i2c_packet = i2c_new_dummy(i2c->adapter, packet_i2c_addr >> 1);
	if (!adv75xx->i2c_packet) {
		HISI_FB_ERR("i2c_new_dummy i2c_packet failed!\n");
		return -ENOMEM;
	}

	adv75xx->i2c_edid = i2c_new_dummy(i2c->adapter, edid_i2c_addr >> 1);
	if (!adv75xx->i2c_edid) {
		HISI_FB_ERR("i2c_new_dummy i2c_edid failed!\n");
		goto err_i2c_unregister_packet;
	}

	adv75xx->i2c_cec = i2c_new_dummy(i2c->adapter, cec_i2c_addr >> 1);
	if (!adv75xx->i2c_cec) {
		ret = -ENOMEM;
		HISI_FB_ERR("i2c_new_dummy i2c_cec failed!\n");
		goto err_i2c_unregister_edid;
	}

	adv75xx->regmap_cec = devm_regmap_init_i2c(adv75xx->i2c_cec,
						   &adv7533_cec_regmap_config);
	if (IS_ERR(adv75xx->regmap_cec)) {
		ret = PTR_ERR(adv75xx->regmap_cec);
		HISI_FB_ERR("devm_regmap_init_i2c regmap_cec failed!\n");
		goto err_i2c_unregister_cec;
	}

	adv75xx->regmap_packet = devm_regmap_init_i2c(adv75xx->i2c_packet,
						      &adv7533_packet_regmap_config);
	if (IS_ERR(adv75xx->regmap_packet)) {
		ret = PTR_ERR(adv75xx->regmap_packet);
		HISI_FB_ERR("devm_regmap_init_i2c regmap_packet failed!\n");
		goto err_i2c_unregister_cec;
	}

	if (adv75xx->type == ADV7533) {
		ret = regmap_register_patch(adv75xx->regmap_cec,
					    adv7533_cec_fixed_registers,
					    ARRAY_SIZE
					    (adv7533_cec_fixed_registers));
		if (ret) {
			HISI_FB_ERR
			    ("regmap_register_patch cec_fixed_registers failed!\n");
			goto err_return;
		}
	}

	HISI_FB_INFO("i2c->irq = %d!\n", i2c->irq);
	if (i2c->irq) {
		init_waitqueue_head(&adv75xx->wq);
		ret = devm_request_threaded_irq(dev, i2c->irq, NULL,
						adv75xx_irq_handler,
						IRQF_ONESHOT, dev_name(dev),
						adv75xx);
		if (ret) {
			HISI_FB_ERR("adv7511_irq_handler registers failed!\n");
			goto err_i2c_unregister_cec;
		}
	}

	/* CEC is unused for now */
	regmap_write(adv75xx->regmap, ADV7533_REG_CEC_CTRL,
		     ADV7533_CEC_CTRL_POWER_DOWN);

	adv75xx_power_off(adv75xx);

	i2c_set_clientdata(i2c, adv75xx);

	/* adv7511_audio_init(dev); */
	status = adv75xx_detect(adv75xx);
	if (status != connector_status_connected) {
		HISI_FB_ERR("adv75xx connector not connected !\n");
	}

	mode = adv75xx_get_modes(adv75xx);

	ret = adv75xx_mode_valid(mode);
	if (ret) {
		HISI_FB_ERR("adv75xx not supported this mode !!\n");
		kfree(mode);
		mode = hisi_set_mode_info();
	}
	adv75xx->mode = mode;

	adv75xx->opt_funcs = &opt_funcs;

	hdmi_pdev =
	    platform_device_alloc("adi_hdmi",
				  (((uint32_t) PANEL_MIPI_VIDEO << 16) |
				   (uint32_t) 1));
	if (hdmi_pdev) {
		if (platform_device_add_data
		    (hdmi_pdev, adv75xx, sizeof(struct adi_hdmi))) {
			HISI_FB_ERR("failed to platform_device_add_data!\n");
			platform_device_put(hdmi_pdev);
		}
	}
	HISI_FB_INFO("platform_device_add_data ok !\n");

	/* set driver data */
	platform_set_drvdata(hdmi_pdev, adv75xx);
	if (platform_device_add(hdmi_pdev)) {
		HISI_FB_ERR("platform_device_add failed!\n");
		goto err_device_put;
	}

	return 0;

 err_i2c_unregister_cec:
	i2c_unregister_device(adv75xx->i2c_cec);
 err_i2c_unregister_edid:
	i2c_unregister_device(adv75xx->i2c_edid);
 err_i2c_unregister_packet:
	i2c_unregister_device(adv75xx->i2c_packet);
 err_device_put:
	platform_device_put(hdmi_pdev);
 err_return:
	kfree(adv75xx);
	return ret;
}

static int adv75xx_remove(struct i2c_client *i2c)
{
	struct adi_hdmi *adv75xx = i2c_get_clientdata(i2c);

	i2c_unregister_device(adv75xx->i2c_cec);
	i2c_unregister_device(adv75xx->i2c_edid);

	kfree(adv75xx->edid);
	kfree(adv75xx->mode);
	kfree(adv75xx);

	return 0;
}

static struct i2c_driver adv75xx_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "adv75xx",
		   .of_match_table = adv75xx_of_ids,
		   },
	.id_table = adv75xx_i2c_ids,
	.probe = adv75xx_probe,
	.remove = adv75xx_remove,
};

static int __init adv75xx_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&adv75xx_driver);
	if (ret) {
		HISI_FB_ERR("i2c_add_driver error!\n");
	}
	return ret;
}

module_init(adv75xx_init);

static void __exit adv75xx_exit(void)
{
	i2c_del_driver(&adv75xx_driver);
}

module_exit(adv75xx_exit);

MODULE_AUTHOR("Hisilicon Inc");
MODULE_DESCRIPTION("ADV75XX HDMI transmitter driver");
MODULE_LICENSE("GPL");

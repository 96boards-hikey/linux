/**
 *
 *
 **/

#ifndef __ADV75XX_H__
#define __ADV75XX_H__
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/hdmi.h>

#include "../hisi_fb.h"

#define DISPLAY_MODE_LEN	32

#define ADV7533_REG_CHIP_REVISION		0x00
#define ADV7533_REG_N0					0x01
#define ADV7533_REG_N1					0x02
#define ADV7533_REG_N2					0x03
#define ADV7533_REG_SPDIF_FREQ			0x04
#define ADV7533_REG_CTS_AUTOMATIC1		0x05
#define ADV7533_REG_CTS_AUTOMATIC2		0x06
#define ADV7533_REG_CTS_MANUAL0			0x07
#define ADV7533_REG_CTS_MANUAL1			0x08
#define ADV7533_REG_CTS_MANUAL2			0x09
#define ADV7533_REG_AUDIO_SOURCE		0x0a
#define ADV7533_REG_AUDIO_CONFIG		0x0b
#define ADV7533_REG_I2S_CONFIG			0x0c
#define ADV7533_REG_I2S_WIDTH			0x0d
#define ADV7533_REG_AUDIO_SUB_SRC0		0x0e
#define ADV7533_REG_AUDIO_SUB_SRC1		0x0f
#define ADV7533_REG_AUDIO_SUB_SRC2		0x10
#define ADV7533_REG_AUDIO_SUB_SRC3		0x11
#define ADV7533_REG_AUDIO_CFG1			0x12
#define ADV7533_REG_AUDIO_CFG2			0x13
#define ADV7533_REG_AUDIO_CFG3			0x14
#define ADV7533_REG_I2C_FREQ_ID_CFG		0x15
#define ADV7533_REG_VIDEO_INPUT_CFG1		0x16
#define ADV7533_REG_CEC_PIXEL_CLOCK_DIV		0x16
#define ADV7533_REG_SYNC_POLARITY		0x17
#define ADV7533_REG_DSI_DATA_LANES		0x1c
#define ADV7533_REG_CSC_UPPER(x)		(0x18 + (x) * 2)
#define ADV7533_REG_CSC_LOWER(x)		(0x19 + (x) * 2)
#define ADV7533_REG_SYNC_DECODER(x)		(0x30 + (x))
#define ADV7533_REG_DE_GENERATOR		(0x35 + (x))
#define ADV7533_REG_PIXEL_REPETITION		0x3b
#define ADV7533_REG_VIC_MANUAL			0x3c
#define ADV7533_REG_VIC_SEND			0x3d
#define ADV7533_REG_VIC_DETECTED		0x3e
#define ADV7533_REG_AUX_VIC_DETECTED		0x3f
#define ADV7533_REG_PACKET_ENABLE0		0x40
#define ADV7533_REG_POWER			0x41
#define ADV7533_REG_STATUS			0x42
#define ADV7533_REG_EDID_I2C_ADDR		0x43
#define ADV7533_REG_PACKET_ENABLE1		0x44
#define ADV7533_REG_PACKET_I2C_ADDR		0x45
#define ADV7533_REG_DSD_ENABLE			0x46
#define ADV7533_REG_VIDEO_INPUT_CFG2		0x48
#define ADV7533_REG_INFOFRAME_UPDATE		0x4a
#define ADV7533_REG_GC(x)			(0x4b + (x))	/* 0x4b - 0x51 */
#define ADV7533_REG_AVI_INFOFRAME_VERSION	0x52
#define ADV7533_REG_AVI_INFOFRAME_LENGTH	0x53
#define ADV7533_REG_AVI_INFOFRAME_CHECKSUM	0x54
#define ADV7533_REG_AVI_INFOFRAME(x)		(0x55 + (x))	/* 0x55 - 0x6f */
#define ADV7533_REG_AUDIO_INFOFRAME_VERSION	0x70
#define ADV7533_REG_AUDIO_INFOFRAME_LENGTH	0x71
#define ADV7533_REG_AUDIO_INFOFRAME_CHECKSUM	0x72
#define ADV7533_REG_AUDIO_INFOFRAME(x)		(0x73 + (x))	/* 0x73 - 0x7c */
#define ADV7533_REG_INT_ENABLE(x)		(0x94 + (x))
#define ADV7533_REG_INT(x)			(0x96 + (x))
#define ADV7533_REG_INPUT_CLK_DIV		0x9d
#define ADV7533_REG_PLL_STATUS			0x9e
#define ADV7533_REG_HDMI_POWER			0xa1
#define ADV7533_REG_HDCP_HDMI_CFG		0xaf
#define ADV7533_REG_AN(x)			(0xb0 + (x))	/* 0xb0 - 0xb7 */
#define ADV7533_REG_HDCP_STATUS			0xb8
#define ADV7533_REG_BCAPS			0xbe
#define ADV7533_REG_BKSV(x)			(0xc0 + (x))	/* 0xc0 - 0xc3 */
#define ADV7533_REG_EDID_SEGMENT		0xc4
#define ADV7533_REG_DDC_STATUS			0xc8
#define ADV7533_REG_EDID_READ_CTRL		0xc9
#define ADV7533_REG_BSTATUS(x)			(0xca + (x))	/* 0xca - 0xcb */
#define ADV7533_REG_TIMING_GEN_SEQ		0xd0
#define ADV7533_REG_POWER2			0xd6
#define ADV7533_REG_HSYNC_PLACEMENT_MSB		0xfa

#define ADV7533_REG_SYNC_ADJUSTMENT(x)		(0xd7 + (x))	/* 0xd7 - 0xdc */
#define ADV7533_REG_TMDS_CLOCK_INV		0xde
#define ADV7533_REG_ARC_CTRL			0xdf
#define ADV7533_REG_CEC_I2C_ADDR		0xe1
#define ADV7533_REG_CEC_CTRL			0xe2
#define ADV7533_REG_CHIP_ID_HIGH		0xf5
#define ADV7533_REG_CHIP_ID_LOW			0xf6

#define ADV7533_CSC_ENABLE			BIT(7)
#define ADV7533_CSC_UPDATE_MODE			BIT(5)

#define ADV7533_INT0_HDP			BIT(7)
#define ADV7533_INT0_VSYNC			BIT(5)
#define ADV7533_INT0_AUDIO_FIFO_FULL		BIT(4)
#define ADV7533_INT0_EDID_READY			BIT(2)
#define ADV7533_INT0_HDCP_AUTHENTICATED		BIT(1)

#define ADV7533_INT1_DDC_ERROR			BIT(7)
#define ADV7533_INT1_BKSV			BIT(6)
#define ADV7533_INT1_CEC_TX_READY		BIT(5)
#define ADV7533_INT1_CEC_TX_ARBIT_LOST		BIT(4)
#define ADV7533_INT1_CEC_TX_RETRY_TIMEOUT	BIT(3)
#define ADV7533_INT1_CEC_RX_READY3		BIT(2)
#define ADV7533_INT1_CEC_RX_READY2		BIT(1)
#define ADV7533_INT1_CEC_RX_READY1		BIT(0)

#define ADV7533_ARC_CTRL_POWER_DOWN		BIT(0)

#define ADV7533_CEC_CTRL_POWER_DOWN		BIT(0)

#define ADV7533_POWER_POWER_DOWN		BIT(6)

#define ADV7533_HDMI_CFG_MODE_MASK		0x2
#define ADV7533_HDMI_CFG_MODE_DVI		0x0
#define ADV7533_HDMI_CFG_MODE_HDMI		0x2

#define ADV7533_AUDIO_SELECT_I2C		0x0
#define ADV7533_AUDIO_SELECT_SPDIF		0x1
#define ADV7533_AUDIO_SELECT_DSD		0x2
#define ADV7533_AUDIO_SELECT_HBR		0x3
#define ADV7533_AUDIO_SELECT_DST		0x4

#define ADV7533_I2S_SAMPLE_LEN_16		0x2
#define ADV7533_I2S_SAMPLE_LEN_20		0x3
#define ADV7533_I2S_SAMPLE_LEN_18		0x4
#define ADV7533_I2S_SAMPLE_LEN_22		0x5
#define ADV7533_I2S_SAMPLE_LEN_19		0x8
#define ADV7533_I2S_SAMPLE_LEN_23		0x9
#define ADV7533_I2S_SAMPLE_LEN_24		0xb
#define ADV7533_I2S_SAMPLE_LEN_17		0xc
#define ADV7533_I2S_SAMPLE_LEN_21		0xd

#define ADV7533_SAMPLE_FREQ_44100		0x0
#define ADV7533_SAMPLE_FREQ_48000		0x2
#define ADV7533_SAMPLE_FREQ_32000		0x3
#define ADV7533_SAMPLE_FREQ_88200		0x8
#define ADV7533_SAMPLE_FREQ_96000		0xa
#define ADV7533_SAMPLE_FREQ_176400		0xc
#define ADV7533_SAMPLE_FREQ_192000		0xe

#define ADV7533_STATUS_POWER_DOWN_POLARITY	BIT(7)
#define ADV7533_STATUS_HPD			BIT(6)
#define ADV7533_STATUS_MONITOR_SENSE		BIT(5)
#define ADV7533_STATUS_I2S_32BIT_MODE		BIT(3)

#define ADV7533_PACKET_ENABLE_N_CTS		BIT(8+6)
#define ADV7533_PACKET_ENABLE_AUDIO_SAMPLE	BIT(8+5)
#define ADV7533_PACKET_ENABLE_AVI_INFOFRAME	BIT(8+4)
#define ADV7533_PACKET_ENABLE_AUDIO_INFOFRAME	BIT(8+3)
#define ADV7533_PACKET_ENABLE_GC		BIT(7)
#define ADV7533_PACKET_ENABLE_SPD		BIT(6)
#define ADV7533_PACKET_ENABLE_MPEG		BIT(5)
#define ADV7533_PACKET_ENABLE_ACP		BIT(4)
#define ADV7533_PACKET_ENABLE_ISRC		BIT(3)
#define ADV7533_PACKET_ENABLE_GM		BIT(2)
#define ADV7533_PACKET_ENABLE_SPARE2		BIT(1)
#define ADV7533_PACKET_ENABLE_SPARE1		BIT(0)

#define ADV7533_REG_POWER2_HDP_SRC_MASK		0xc0
#define ADV7533_REG_POWER2_HDP_SRC_BOTH		0x00
#define ADV7533_REG_POWER2_HDP_SRC_HDP		0x40
#define ADV7533_REG_POWER2_HDP_SRC_CEC		0x80
#define ADV7533_REG_POWER2_HDP_SRC_NONE		0xc0
#define ADV7533_REG_POWER2_TDMS_ENABLE		BIT(4)
#define ADV7533_REG_POWER2_GATE_INPUT_CLK	BIT(0)

#define ADV7533_LOW_REFRESH_RATE_NONE		0x0
#define ADV7533_LOW_REFRESH_RATE_24HZ		0x1
#define ADV7533_LOW_REFRESH_RATE_25HZ		0x2
#define ADV7533_LOW_REFRESH_RATE_30HZ		0x3

#define ADV7533_AUDIO_CFG3_LEN_MASK		0x0f
#define ADV7533_I2C_FREQ_ID_CFG_RATE_MASK	0xf0

#define ADV7533_AUDIO_SOURCE_I2S		0
#define ADV7533_AUDIO_SOURCE_SPDIF		1

#define ADV7533_I2S_FORMAT_I2S			0
#define ADV7533_I2S_FORMAT_RIGHT_J		1
#define ADV7533_I2S_FORMAT_LEFT_J		2

#define ADV7533_PACKET(p, x)	    ((p) * 0x20 + (x))
#define ADV7533_PACKET_SDP(x)	    ADV7533_PACKET(0, x)
#define ADV7533_PACKET_MPEG(x)	    ADV7533_PACKET(1, x)
#define ADV7533_PACKET_ACP(x)	    ADV7533_PACKET(2, x)
#define ADV7533_PACKET_ISRC1(x)	    ADV7533_PACKET(3, x)
#define ADV7533_PACKET_ISRC2(x)	    ADV7533_PACKET(4, x)
#define ADV7533_PACKET_GM(x)	    ADV7533_PACKET(5, x)
#define ADV7533_PACKET_SPARE(x)	    ADV7533_PACKET(6, x)

#define EDID_LENGTH				0x80
#define EDID_EXTENSION_NUM				0x7e

/* Video mode flags */
/* bit compatible with the xorg definitions. */
#define MODE_FLAG_PHSYNC			(1<<0)
#define MODE_FLAG_NHSYNC			(1<<1)
#define MODE_FLAG_PVSYNC			(1<<2)
#define MODE_FLAG_NVSYNC			(1<<3)
#define MODE_FLAG_INTERLACE			(1<<4)
#define MODE_FLAG_DBLSCAN			(1<<5)
#define MODE_FLAG_CSYNC			(1<<6)
#define MODE_FLAG_PCSYNC			(1<<7)
#define MODE_FLAG_NCSYNC			(1<<8)
#define MODE_FLAG_HSKEW			(1<<9)	/* hskew provided */
#define MODE_FLAG_BCAST			(1<<10)
#define MODE_FLAG_PIXMUX			(1<<11)
#define MODE_FLAG_DBLCLK			(1<<12)
#define MODE_FLAG_CLKDIV2			(1<<13)

/*
 * Note on terminology:  here, for brevity and convenience, we refer to connector
 * control chips as 'CRTCs'.  They can control any type of connector, VGA, LVDS,
 * DVI, etc.  And 'screen' refers to the whole of the visible display, which
 * may span multiple monitors (and therefore multiple CRTC and connector
 * structures).
 */

enum mode_status {
	MODE_OK = 0,		/* Mode OK */
	MODE_HSYNC,		/* hsync out of range */
	MODE_VSYNC,		/* vsync out of range */
	MODE_H_ILLEGAL,		/* mode has illegal horizontal timings */
	MODE_V_ILLEGAL,		/* mode has illegal horizontal timings */
	MODE_BAD_WIDTH,		/* requires an unsupported linepitch */
	MODE_NOMODE,		/* no mode with a matching name */
	MODE_NO_INTERLACE,	/* interlaced mode not supported */
	MODE_NO_DBLESCAN,	/* doublescan mode not supported */
	MODE_NO_VSCAN,		/* multiscan mode not supported */
	MODE_MEM,		/* insufficient video memory */
	MODE_VIRTUAL_X,		/* mode width too large for specified virtual size */
	MODE_VIRTUAL_Y,		/* mode height too large for specified virtual size */
	MODE_MEM_VIRT,		/* insufficient video memory given virtual size */
	MODE_NOCLOCK,		/* no fixed clock available */
	MODE_CLOCK_HIGH,	/* clock required is too high */
	MODE_CLOCK_LOW,		/* clock required is too low */
	MODE_CLOCK_RANGE,	/* clock/mode isn't in a ClockRange */
	MODE_BAD_HVALUE,	/* horizontal timing was out of range */
	MODE_BAD_VVALUE,	/* vertical timing was out of range */
	MODE_BAD_VSCAN,		/* VScan value out of range */
	MODE_HSYNC_NARROW,	/* horizontal sync too narrow */
	MODE_HSYNC_WIDE,	/* horizontal sync too wide */
	MODE_HBLANK_NARROW,	/* horizontal blanking too narrow */
	MODE_HBLANK_WIDE,	/* horizontal blanking too wide */
	MODE_VSYNC_NARROW,	/* vertical sync too narrow */
	MODE_VSYNC_WIDE,	/* vertical sync too wide */
	MODE_VBLANK_NARROW,	/* vertical blanking too narrow */
	MODE_VBLANK_WIDE,	/* vertical blanking too wide */
	MODE_PANEL,		/* exceeds panel dimensions */
	MODE_INTERLACE_WIDTH,	/* width too large for interlaced mode */
	MODE_ONE_WIDTH,		/* only one width is supported */
	MODE_ONE_HEIGHT,	/* only one height is supported */
	MODE_ONE_SIZE,		/* only one resolution is supported */
	MODE_NO_REDUCED,	/* monitor doesn't accept reduced blanking */
	MODE_NO_STEREO,		/* stereo modes not supported */
	MODE_UNVERIFIED = -3,	/* mode needs to reverified */
	MODE_BAD = -2,		/* unspecified reason */
	MODE_ERROR = -1		/* error condition */
};

enum DDC_controller_status {
	IN_RESET = 0,		/* In Reset (No Hot Plug Detected) */
	READING_EDID,		/* Reading EDID */
	IDLE,			/* IDLE (Waiting for HDCP Requested) */
	INIT_HDCP,		/* Initializing HDCP */
	HDCP_ENABLE,		/* HDCP Enabled */
	INIT_HDCP_REPEAT	/* Initializing HDCP Repeater */
};

/* If detailed data is pixel timing */
struct detailed_pixel_timing {
	u8 hactive_lo;
	u8 hblank_lo;
	u8 hactive_hblank_hi;
	u8 vactive_lo;
	u8 vblank_lo;
	u8 vactive_vblank_hi;
	u8 hsync_offset_lo;
	u8 hsync_pulse_width_lo;
	u8 vsync_offset_pulse_width_lo;
	u8 hsync_vsync_offset_pulse_width_hi;
	u8 width_mm_lo;
	u8 height_mm_lo;
	u8 width_height_mm_hi;
	u8 hborder;
	u8 vborder;
	u8 misc;
} __attribute__ ((packed));

struct est_timings {
	u8 t1;
	u8 t2;
	u8 mfg_rsvd;
} __attribute__ ((packed));

struct std_timing {
	u8 hsize;		/* need to multiply by 8 then add 248 */
	u8 vfreq_aspect;
} __attribute__ ((packed));

struct detailed_timing {
	__le16 pixel_clock;	/* need to multiply by 10 KHz */
	union {
		struct detailed_pixel_timing pixel_data;

	} data;
} __attribute__ ((packed));

struct edid {
	u8 header[8];
	/* Vendor & product info */
	u8 mfg_id[2];
	u8 prod_code[2];
	u32 serial;		/* FIXME: byte order */
	u8 mfg_week;
	u8 mfg_year;
	/* EDID version */
	u8 version;
	u8 revision;
	/* Display info: */
	u8 input;
	u8 width_cm;
	u8 height_cm;
	u8 gamma;
	u8 features;
	/* Color characteristics */
	u8 red_green_lo;
	u8 black_white_lo;
	u8 red_x;
	u8 red_y;
	u8 green_x;
	u8 green_y;
	u8 blue_x;
	u8 blue_y;
	u8 white_x;
	u8 white_y;
	/* Est. timings and mfg rsvd timings */
	struct est_timings established_timings;
	/* Standard timings 1-8 */
	struct std_timing standard_timings[8];
	/* Detailing timings 1-4 */
	struct detailed_timing detailed_timings[4];
	/* Number of 128 byte ext. blocks */
	u8 extensions;
	/* Checksum */
	u8 checksum;
} __attribute__ ((packed));

/**
 * enum adv75xx_csc_scaling - Scaling factor for the ADV75xx CSC
 * @ADV75xx_CSC_SCALING_1: CSC results are not scaled
 * @ADV75xx_CSC_SCALING_2: CSC results are scaled by a factor of two
 * @ADV75xx_CSC_SCALING_4: CSC results are scalled by a factor of four
 */
enum adv75xx_csc_scaling {
	ADV75xx_CSC_SCALING_1 = 0,
	ADV75xx_CSC_SCALING_2 = 1,
	ADV75xx_CSC_SCALING_4 = 2,
};

/**
 * struct adv75xx_video_config - Describes adv75xx hardware configuration
 * @csc_enable:			Whether to enable color space conversion
 * @csc_scaling_factor:		Color space conversion scaling factor
 * @csc_coefficents:		Color space conversion coefficents
 * @hdmi_mode:			Whether to use HDMI or DVI output mode
 * @avi_infoframe:		HDMI infoframe
 */
struct adv75xx_video_config {
	bool csc_enable;
	enum adv75xx_csc_scaling csc_scaling_factor;
	const uint16_t *csc_coefficents;

	bool hdmi_mode;
	struct hdmi_avi_infoframe avi_infoframe;
};

struct hisi_display_mode {

	unsigned int type;

	/* Proposed mode values */
	int clock;		/* in kHz */
	int hdisplay;
	int hsync_start;
	int hsync_end;
	int hsync_pulse_width;
	int hsync_offset;
	int htotal;

	int vdisplay;
	int vsync_start;
	int vsync_end;
	int vsync_pulse_width;
	int vsync_offset;
	int vtotal;
	int vscan;
	unsigned int flags;

	/* Addressable image size (may be 0 for projectors, etc.) */
	int width_mm;
	int height_mm;

	int vrefresh;		/* in Hz */
	int hsync;		/* in kHz */
	enum hdmi_picture_aspect picture_aspect_ratio;
};

/**
 * enum adv7511_sync_polarity - Polarity for the input sync signals
 * @ADV7533_SYNC_POLARITY_PASSTHROUGH:  Sync polarity matches that of
 *				       the currently configured mode.
 * @ADV7533_SYNC_POLARITY_LOW:	    Sync polarity is low
 * @ADV7533_SYNC_POLARITY_HIGH:	    Sync polarity is high
 *
 * If the polarity is set to either LOW or HIGH the driver will configure the
 * ADV7533 to internally invert the sync signal if required to match the sync
 * polarity setting for the currently selected output mode.
 *
 * If the polarity is set to PASSTHROUGH, the ADV7533 will route the signal
 * unchanged. This is used when the upstream graphics core already generates
 * the sync signals with the correct polarity.
 */
enum adi_sync_polarity {
	ADV7533_SYNC_POLARITY_PASSTHROUGH,
	ADV7533_SYNC_POLARITY_LOW,
	ADV7533_SYNC_POLARITY_HIGH,
};

enum adv75xx_type {
	ADV7511,
	ADV7533,
	ADV7535,
};

enum connector_status {
	connector_status_connected = 1,
	connector_status_disconnected = 2,
	connector_status_unknown = 3,
};

struct adi_hdmi {
	enum adv75xx_type type;
	bool powered;

	struct regulator *vdd;
	struct regulator *v1p2;

	struct i2c_client *i2c_main;
	struct i2c_client *i2c_edid;
	struct i2c_client *i2c_cec;
	struct i2c_client *i2c_packet;

	struct regmap *regmap;
	struct regmap *regmap_cec;
	struct regmap *regmap_packet;
	enum connector_status status;

	unsigned int f_tmds;
	unsigned int f_audio;
	unsigned int audio_source;

	bool edid_read;
	unsigned int current_edid_segment;

	wait_queue_head_t wq;

	bool rgb;
	bool embedded_sync;
	enum adi_sync_polarity vsync_polarity;
	enum adi_sync_polarity hsync_polarity;
	uint8_t num_dsi_lanes;

	struct edid *edid;
	struct gpio_desc *gpio_pd;

	struct hisi_display_mode *mode;
	struct adi_operation_funcs *opt_funcs;
};

struct adi_operation_funcs {
	void (*power_on)(struct adi_hdmi *adv75xx);
	void (*power_off)(struct adi_hdmi *adv75xx);
	void (*mode_set)(struct adi_hdmi *adv75xx,
			  struct hisi_display_mode *mode);
};

#endif				/* __ADV75XX_H__ */

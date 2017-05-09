ifeq ($(CONFIG_HISI_FB_KIRIN960),y)
obj-$(CONFIG_HISI_FB_KIRIN960) := hisifb.o
endif

hisifb-objs := \
	hisi_mipi_dsi_host.o \
	hisi_mipi_dsi.o \
	hisi_dpe.o \
	hisi_fb_panel.o \
	hisi_fb_isr.o \
	hisi_fb_vsync.o \
	hisi_fb_buf_sync.o \
	hisi_fb_bl.o \
	hisi_fb_utils.o \
	hisi_fb.o \
	hisi_overlay_utils.o \
	hisi_block_algorithm.o \
	hisi_overlay_online.o \
	hisi_overlay_cmdlist_utils.o

ifeq ($(CONFIG_HISI_FB_KIRIN960),y)
hisifb-objs += \
	hisi_dpe_utils_hi3660.o \
	hisi_overlay_utils_hi3660.o \
	panel/mipi_hikey_nte300nts.o \
	hdmi/adv75xx.o \
	hdmi/mipi_adi_hdmi.o
endif

EXTRA_CFLAGS += -Idrivers/video/hisi/dss \
		-Idrivers/video/hisi/panel \
		-Idrivers/video/hisi \
		-Iinclude \
		-Idrivers/staging/android

ifeq ($(CONFIG_HISI_PERIDVFS),y)
EXTRA_CFLAGS += -Idrivers/clk/hisi/peri_dvfs
endif

clean:
	rm *.o .*cmd

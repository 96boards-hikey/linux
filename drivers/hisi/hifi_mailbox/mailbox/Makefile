ccflags-y += -I$(srctree)/drivers/hisi/hifi_mailbox/ipcm
ccflags-y += -I$(srctree)/drivers/hisi/hifi_mailbox/mailbox

ccflags-y += -DBSP_CORE_APP
ifdef CONFIG_HIFI_RESET
ccflags-y += -D_HIFI_WD_DEBUG
endif

ccflags-y += -Idrivers/hisi/hifidsp

obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_table.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_gut.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_ifc.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_msg.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_debug.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_port_linux.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox_ifc.o
obj-$(CONFIG_HIFI_MAILBOX)	+= drv_mailbox.o

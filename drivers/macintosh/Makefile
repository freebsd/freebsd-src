#
# Makefile for the Macintosh-specific device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

# The target object and module list name.

O_TARGET	:= macintosh.o

# Objects that export symbols.

export-objs	:= adb.o rtc.o mac_hid.o via-pmu.o

# Object file lists.

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

# Each configuration option enables a list of files.

obj-$(CONFIG_PMAC_PBOOK)	+= mediabay.o
obj-$(CONFIG_MAC_SERIAL)	+= macserial.o
ifneq ($(CONFIG_MAC),y)
  obj-$(CONFIG_NVRAM)		+= nvram.o
endif
obj-$(CONFIG_MAC_HID)		+= mac_hid.o
obj-$(CONFIG_INPUT_ADBHID)	+= adbhid.o
obj-$(CONFIG_PPC_RTC)		+= rtc.o
obj-$(CONFIG_ANSLCD)		+= ans-lcd.o

obj-$(CONFIG_ADB_PMU)		+= via-pmu.o
obj-$(CONFIG_ADB_CUDA)		+= via-cuda.o
obj-$(CONFIG_PMAC_APM_EMU)	+= apm_emu.o

obj-$(CONFIG_ADB)		+= adb.o
obj-$(CONFIG_ADB_KEYBOARD)	+= mac_keyb.o
obj-$(CONFIG_ADB_MACII)		+= via-macii.o
obj-$(CONFIG_ADB_MACIISI)	+= via-maciisi.o
obj-$(CONFIG_ADB_IOP)		+= adb-iop.o
obj-$(CONFIG_ADB_PMU68K)	+= via-pmu68k.o
obj-$(CONFIG_ADB_MACIO)		+= macio-adb.o

# The global Rules.make.

include $(TOPDIR)/Rules.make


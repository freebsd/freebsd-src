#
# Makefile for the video capture/playback device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

# Object file lists.

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

SUB_DIRS     := 
MOD_SUB_DIRS := $(SUB_DIRS)
ALL_SUB_DIRS := $(SUB_DIRS)

O_TARGET := video.o

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs     :=	i2c-old.o videodev.o bttv-if.o cpia.o

list-multi	:=	bttv.o zoran.o
bttv-objs	:=	bttv-driver.o bttv-cards.o bttv-if.o
zoran-objs      :=	zr36120.o zr36120_i2c.o zr36120_mem.o

obj-$(CONFIG_VIDEO_DEV) += videodev.o

obj-$(CONFIG_BUS_I2C) += i2c-old.o
obj-$(CONFIG_VIDEO_BT848) += bttv.o msp3400.o tvaudio.o \
	tda7432.o tda9875.o tda9887.o tuner.o
obj-$(CONFIG_SOUND_TVMIXER) += tvmixer.o

obj-$(CONFIG_VIDEO_ZR36120) += zoran.o i2c-old.o
obj-$(CONFIG_I2C_PARPORT) += i2c-parport.o i2c-old.o
obj-$(CONFIG_VIDEO_SAA5249) += saa5249.o
obj-$(CONFIG_VIDEO_CQCAM) += c-qcam.o
obj-$(CONFIG_VIDEO_BWQCAM) += bw-qcam.o
obj-$(CONFIG_VIDEO_W9966) += w9966.o
obj-$(CONFIG_VIDEO_ZORAN) += zr36067.o i2c-old.o
obj-$(CONFIG_VIDEO_ZORAN_BUZ) += saa7111.o saa7185.o
obj-$(CONFIG_VIDEO_ZORAN_DC10) += saa7110.o adv7175.o
obj-$(CONFIG_VIDEO_ZORAN_LML33) += bt819.o bt856.o
obj-$(CONFIG_VIDEO_PMS) += pms.o
obj-$(CONFIG_VIDEO_PLANB) += planb.o
obj-$(CONFIG_VIDEO_VINO) += saa7191.o indycam.o vino.o
obj-$(CONFIG_VIDEO_STRADIS) += stradis.o i2c-old.o
obj-$(CONFIG_VIDEO_CPIA) += cpia.o
obj-$(CONFIG_VIDEO_CPIA_PP) += cpia_pp.o
obj-$(CONFIG_VIDEO_CPIA_USB) += cpia_usb.o
obj-$(CONFIG_VIDEO_MEYE) += meye.o
obj-$(CONFIG_VIDEO_SWARM_7114H) += swarm_saa7114h.o
obj-$(CONFIG_TUNER_3036) += tuner-3036.o

# Extract lists of the multi-part drivers.
# The 'int-*' lists are the intermediate files used to build the multi's.

multi-y         := $(filter $(list-multi), $(obj-y))
multi-m         := $(filter $(list-multi), $(obj-m))
int-y           := $(sort $(foreach m, $(multi-y), $($(basename $(m))-objs)))
int-m           := $(sort $(foreach m, $(multi-m), $($(basename $(m))-objs)))

# Files that are both resident and modular: remove from modular.

obj-m           := $(filter-out $(obj-y), $(obj-m))
int-m           := $(filter-out $(int-y), $(int-m))

# Take multi-part drivers out of obj-y and put components in.

obj-y           := $(filter-out $(list-multi), $(obj-y)) $(int-y)

include $(TOPDIR)/Rules.make

fastdep:

zoran.o: $(zoran-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(zoran-objs)

bttv.o: $(bttv-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(bttv-objs)

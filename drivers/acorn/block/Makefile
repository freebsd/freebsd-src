#
# Makefile for the Acorn block device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now inherited from the
# parent makefile.
#

USE_STANDARD_AS_RULE := true

O_TARGET	:= acorn-block.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

export-objs	:=
list-multi	:= fd1772_mod.o mfmhd_mod.o
fd1772_mod-objs	:= fd1772.o fd1772dma.o
mfmhd_mod-objs	:= mfmhd.o mfm.o

obj-$(CONFIG_BLK_DEV_FD1772)	+= fd1772_mod.o
obj-$(CONFIG_BLK_DEV_MFM)	+= mfmhd_mod.o

include $(TOPDIR)/Rules.make

fd1772_mod.o: $(fd1772_mod-objs)
	$(LD) -r -o $@ $(fd1772_mod-objs)

mfmhd_mod.o: $(mfmhd_mod-objs)
	$(LD) -r -o $@ $(mfmhd_mod-objs)

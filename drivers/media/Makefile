#
# Makefile for the kernel multimedia device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

subdir-y     := video radio
mod-subdirs  := video radio

O_TARGET     := media.o
obj-y        := $(join $(subdir-y),$(subdir-y:%=/%.o))

include $(TOPDIR)/Rules.make

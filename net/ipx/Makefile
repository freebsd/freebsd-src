#
# Makefile for the Linux IPX layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

# We only get in/to here if CONFIG_IPX = 'y' or 'm'

O_TARGET := ipx.o

export-objs = af_ipx.o af_spx.o

obj-y	:= af_ipx.o

ifeq ($(CONFIG_IPX),m)
  obj-m += $(O_TARGET)
endif

obj-$(CONFIG_SYSCTL) += sysctl_net_ipx.o
obj-$(CONFIG_SPX) += af_spx.o

include $(TOPDIR)/Rules.make

#
# Makefile for the Linux AppleTalk layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := appletalk.o

export-objs = ddp.o

obj-y	 := aarp.o ddp.o
obj-m	 := $(O_TARGET)

obj-$(CONFIG_SYSCTL) += sysctl_net_atalk.o

include $(TOPDIR)/Rules.make


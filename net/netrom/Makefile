#
# Makefile for the Linux NET/ROM layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := netrom.o

obj-y	 := af_netrom.o nr_dev.o nr_in.o nr_loopback.o nr_out.o nr_route.o \
	    nr_subr.o nr_timer.o
obj-m    := $(O_TARGET)

obj-$(CONFIG_SYSCTL) += sysctl_net_netrom.o

include $(TOPDIR)/Rules.make


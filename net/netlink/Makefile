#
# Makefile for the netlink driver.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := netlink.o

export-objs := af_netlink.o

obj-y  := af_netlink.o

obj-m  :=

obj-$(CONFIG_NETLINK_DEV) += netlink_dev.o

include $(TOPDIR)/Rules.make

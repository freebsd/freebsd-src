#
# Makefile for the Linux 802.x protocol layers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := 802.o

export-objs = llc_macinit.o p8022.o psnap.o fc.o

obj-y	= p8023.o

obj-$(CONFIG_SYSCTL) += sysctl_net_802.o
obj-$(CONFIG_LLC) += llc_sendpdu.o llc_utility.o cl2llc.o llc_macinit.o
ifeq ($(CONFIG_SYSCTL),y)
obj-y += sysctl_net_802.o
endif

ifeq ($(CONFIG_LLC),y)
subdir-y += transit
obj-y += llc_sendpdu.o llc_utility.o cl2llc.o llc_macinit.o
SNAP = y
endif

ifdef CONFIG_TR
obj-y += tr.o
	SNAP=y
endif

ifdef CONFIG_NET_FC
obj-y += fc.o
endif

ifdef CONFIG_FDDI
obj-y += fddi.o
endif

ifdef CONFIG_HIPPI
obj-y += hippi.o
endif

ifdef CONFIG_IPX
	SNAP=y
endif

ifdef CONFIG_ATALK
	SNAP=y
endif

ifeq ($(SNAP),y)
obj-y += p8022.o psnap.o
endif

include $(TOPDIR)/Rules.make

cl2llc.c: cl2llc.pre
	@rm -f $@
	sed -f ./pseudo/opcd2num.sed cl2llc.pre >cl2llc.c

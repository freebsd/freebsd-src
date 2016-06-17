#
# Makefile for the Linux networking core.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := core.o

export-objs := netfilter.o profile.o

obj-y := sock.o skbuff.o iovec.o datagram.o scm.o

ifeq ($(CONFIG_SYSCTL),y)
ifeq ($(CONFIG_NET),y)
obj-y += sysctl_net_core.o
endif
endif

obj-$(CONFIG_FILTER) += filter.o

obj-$(CONFIG_NET) +=	dev.o ethtool.o dev_mcast.o dst.o neighbour.o \
			rtnetlink.o utils.o

obj-$(CONFIG_NETFILTER) += netfilter.o
obj-$(CONFIG_NET_DIVERT) += dv.o
obj-$(CONFIG_NET_PROFILE) += profile.o
obj-$(CONFIG_NET_PKTGEN) += pktgen.o
obj-$(CONFIG_NET_RADIO) += wireless.o
# Ugly. I wish all wireless drivers were moved in drivers/net/wireless
obj-$(CONFIG_NET_PCMCIA_RADIO) += wireless.o

include $(TOPDIR)/Rules.make

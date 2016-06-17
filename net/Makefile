#
# Makefile for the linux networking.
#
# 2 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
#

O_TARGET :=	network.o

mod-subdirs :=	ipv4/netfilter ipv6/netfilter ipx irda bluetooth atm netlink sched core sctp
export-objs :=	netsyms.o

subdir-y :=	core ethernet
subdir-m :=	ipv4 # hum?


subdir-$(CONFIG_NET)		+= 802 sched netlink
subdir-$(CONFIG_IPV6)		+= ipv6
subdir-$(CONFIG_INET)		+= ipv4
subdir-$(CONFIG_NETFILTER)	+= ipv4/netfilter
subdir-$(CONFIG_UNIX)		+= unix
subdir-$(CONFIG_IP_SCTP)	+= sctp

ifneq ($(CONFIG_IPV6),n)
ifneq ($(CONFIG_IPV6),)
subdir-$(CONFIG_NETFILTER)	+= ipv6/netfilter
endif
endif

subdir-$(CONFIG_KHTTPD)		+= khttpd
subdir-$(CONFIG_PACKET)		+= packet
subdir-$(CONFIG_NET_SCHED)	+= sched
subdir-$(CONFIG_BRIDGE)		+= bridge
subdir-$(CONFIG_IPX)		+= ipx
subdir-$(CONFIG_ATALK)		+= appletalk
subdir-$(CONFIG_WAN_ROUTER)	+= wanrouter
subdir-$(CONFIG_X25)		+= x25
subdir-$(CONFIG_LAPB)		+= lapb
subdir-$(CONFIG_NETROM)		+= netrom
subdir-$(CONFIG_ROSE)		+= rose
subdir-$(CONFIG_AX25)		+= ax25
subdir-$(CONFIG_IRDA)		+= irda
subdir-$(CONFIG_BLUEZ)		+= bluetooth
subdir-$(CONFIG_SUNRPC)		+= sunrpc
subdir-$(CONFIG_ATM)		+= atm
subdir-$(CONFIG_DECNET)		+= decnet
subdir-$(CONFIG_ECONET)		+= econet
subdir-$(CONFIG_VLAN_8021Q)           += 8021q

ifeq ($(CONFIG_NETFILTER),y)
  mod-subdirs += ipv4/ipvs
  subdir-$(CONFIG_IP_VS) += ipv4/ipvs
endif

obj-y	:= socket.o $(join $(subdir-y), $(patsubst %,/%.o,$(notdir $(subdir-y))))
ifeq ($(CONFIG_NET),y)
obj-$(CONFIG_MODULES)		+= netsyms.o
obj-$(CONFIG_SYSCTL)		+= sysctl_net.o
endif

include $(TOPDIR)/Rules.make

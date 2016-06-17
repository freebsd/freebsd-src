#
# Makefile for the Linux TCP/IP (INET) layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := ipv4.o

export-objs = ipip.o ip_gre.o

obj-y     := utils.o route.o inetpeer.o proc.o protocol.o \
	     ip_input.o ip_fragment.o ip_forward.o ip_options.o \
	     ip_output.o ip_sockglue.o \
	     tcp.o tcp_input.o tcp_output.o tcp_timer.o tcp_ipv4.o tcp_minisocks.o \
	     tcp_diag.o raw.o udp.o arp.o icmp.o devinet.o af_inet.o igmp.o \
	     sysctl_net_ipv4.o fib_frontend.o fib_semantics.o fib_hash.o

obj-$(CONFIG_IP_MULTIPLE_TABLES) += fib_rules.o
obj-$(CONFIG_IP_ROUTE_NAT) += ip_nat_dumb.o
obj-$(CONFIG_IP_MROUTE) += ipmr.o
obj-$(CONFIG_NET_IPIP) += ipip.o
obj-$(CONFIG_NET_IPGRE) += ip_gre.o
obj-$(CONFIG_SYN_COOKIES) += syncookies.o
obj-$(CONFIG_IP_PNP) += ipconfig.o

include $(TOPDIR)/Rules.make

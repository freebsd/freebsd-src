# $FreeBSD: src/sys/modules/firewire/fwe/Makefile,v 1.2.2.3 2003/07/25 05:01:39 simokawa Exp $

# Makefile for the fwe(4) module (Ethernet over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= if_fwe
SRCS	= bus_if.h device_if.h \
	  opt_inet.h \
	  if_fwe.c if_fwevar.h \
	  firewire.h firewirereg.h
#CFLAGS += -DDEVICE_POLLING
KMODDEPS = firewire

.include <bsd.kmod.mk>


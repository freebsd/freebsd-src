# $FreeBSD: src/sys/modules/firewire/fwe/Makefile,v 1.3.24.1 2008/10/02 02:57:24 kensmith Exp $

# Makefile for the fwe(4) module (Ethernet over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= if_fwe
SRCS	= bus_if.h device_if.h \
	  opt_inet.h \
	  if_fwe.c if_fwevar.h \
	  firewire.h firewirereg.h
#CFLAGS += -DDEVICE_POLLING

.include <bsd.kmod.mk>


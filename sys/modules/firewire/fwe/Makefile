# $FreeBSD: src/sys/modules/firewire/fwe/Makefile,v 1.2 2002/10/14 19:18:30 jhb Exp $

# Makefile for the fwe(4) module (Ethernet over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= if_fwe
SRCS	= bus_if.h device_if.h \
	  opt_inet.h \
	  if_fwe.c if_fwevar.h \
	  firewire.h firewirereg.h

.include <bsd.kmod.mk>


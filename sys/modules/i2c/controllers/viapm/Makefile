# $FreeBSD: src/sys/modules/i2c/controllers/viapm/Makefile,v 1.1.14.2.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= viapm
SRCS		= device_if.h bus_if.h isa_if.h iicbb_if.h pci_if.h smbus_if.h \
		  opt_isa.h viapm.c

.include <bsd.kmod.mk>

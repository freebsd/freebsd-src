# $FreeBSD: src/sys/modules/i2c/controllers/viapm/Makefile,v 1.3.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= viapm
SRCS		= device_if.h bus_if.h iicbb_if.h isa_if.h pci_if.h smbus_if.h \
		  opt_isa.h viapm.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/i2c/controllers/alpm/Makefile,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= alpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  alpm.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/i2c/controllers/alpm/Makefile,v 1.1.14.1.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= alpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  alpm.c

.include <bsd.kmod.mk>

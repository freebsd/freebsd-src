# $FreeBSD: src/sys/modules/i2c/controllers/alpm/Makefile,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= alpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  alpm.c

.include <bsd.kmod.mk>

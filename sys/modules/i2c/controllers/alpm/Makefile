# $FreeBSD: src/sys/modules/i2c/controllers/alpm/Makefile,v 1.2.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= alpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  alpm.c

.include <bsd.kmod.mk>

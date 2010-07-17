# $FreeBSD: src/sys/modules/i2c/controllers/amdsmb/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../pci

KMOD=	amdsmb
SRCS=	amdsmb.c
SRCS+=	device_if.h smbus_if.h pci_if.h bus_if.h

.include <bsd.kmod.mk>

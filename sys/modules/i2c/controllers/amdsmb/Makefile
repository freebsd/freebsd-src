# $FreeBSD: src/sys/modules/i2c/controllers/amdsmb/Makefile,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../pci

KMOD=	amdsmb
SRCS=	amdsmb.c
SRCS+=	device_if.h smbus_if.h pci_if.h bus_if.h

.include <bsd.kmod.mk>

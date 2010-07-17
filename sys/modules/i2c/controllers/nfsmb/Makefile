# $FreeBSD: src/sys/modules/i2c/controllers/nfsmb/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../pci

KMOD=	nfsmb
SRCS=	nfsmb.c
SRCS+=	device_if.h smbus_if.h pci_if.h bus_if.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/i2c/controllers/nfsmb/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../pci

KMOD=	nfsmb
SRCS=	nfsmb.c
SRCS+=	device_if.h smbus_if.h pci_if.h bus_if.h

.include <bsd.kmod.mk>

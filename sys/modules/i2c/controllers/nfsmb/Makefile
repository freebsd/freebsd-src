# $FreeBSD: src/sys/modules/i2c/controllers/nfsmb/Makefile,v 1.1.16.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../pci

KMOD=	nfsmb
SRCS=	nfsmb.c
SRCS+=	device_if.h smbus_if.h pci_if.h bus_if.h

.include <bsd.kmod.mk>

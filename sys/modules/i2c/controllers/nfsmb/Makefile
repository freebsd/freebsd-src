# $FreeBSD: src/sys/modules/i2c/controllers/nfsmb/Makefile,v 1.1.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../pci

KMOD=	nfsmb
SRCS=	nfsmb.c
SRCS+=	device_if.h smbus_if.h pci_if.h bus_if.h

.include <bsd.kmod.mk>

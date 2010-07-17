# $FreeBSD: src/sys/modules/dpms/Makefile,v 1.1.4.2.2.1 2010/06/14 02:09:06 kensmith Exp $

KMOD=	dpms
SRCS=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../dev/dpms
SRCS+=	dpms.c

.include <bsd.kmod.mk>

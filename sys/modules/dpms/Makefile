# $FreeBSD: src/sys/modules/dpms/Makefile,v 1.1.4.2.4.1 2010/12/21 17:09:25 kensmith Exp $

KMOD=	dpms
SRCS=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../dev/dpms
SRCS+=	dpms.c

.include <bsd.kmod.mk>

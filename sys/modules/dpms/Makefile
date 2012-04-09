# $FreeBSD: src/sys/modules/dpms/Makefile,v 1.1.4.2.6.1 2012/03/03 06:15:13 kensmith Exp $

KMOD=	dpms
SRCS=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../dev/dpms
SRCS+=	dpms.c

.include <bsd.kmod.mk>

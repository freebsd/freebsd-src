# $FreeBSD: src/sys/modules/cm/Makefile,v 1.1.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/cm

KMOD=	if_cm
SRCS=	smc90cx6.c if_cm_isa.c

SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

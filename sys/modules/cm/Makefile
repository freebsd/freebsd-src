# $FreeBSD: src/sys/modules/cm/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/cm

KMOD=	if_cm
SRCS=	smc90cx6.c if_cm_isa.c

SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

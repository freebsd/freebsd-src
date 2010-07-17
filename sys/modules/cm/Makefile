# $FreeBSD: src/sys/modules/cm/Makefile,v 1.1.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/cm

KMOD=	if_cm
SRCS=	smc90cx6.c if_cm_isa.c

SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

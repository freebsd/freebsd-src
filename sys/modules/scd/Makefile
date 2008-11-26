# $FreeBSD: src/sys/modules/scd/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/scd

KMOD=	scd
SRCS=	scd.c scd_isa.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

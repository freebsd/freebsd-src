# $FreeBSD: src/sys/modules/scd/Makefile,v 1.1.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/scd

KMOD=	scd
SRCS=	scd.c scd_isa.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

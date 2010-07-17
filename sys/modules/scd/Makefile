# $FreeBSD: src/sys/modules/scd/Makefile,v 1.1.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/scd

KMOD=	scd
SRCS=	scd.c scd_isa.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

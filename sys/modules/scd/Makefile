# $FreeBSD: src/sys/modules/scd/Makefile,v 1.1.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/scd

KMOD=	scd
SRCS=	scd.c scd_isa.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

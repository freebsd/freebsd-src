# $FreeBSD: src/sys/modules/scd/Makefile,v 1.1 2002/11/05 09:37:32 mdodd Exp $

.PATH: ${.CURDIR}/../../dev/scd

KMOD=	scd
SRCS=	scd.c scd_isa.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

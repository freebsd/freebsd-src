# $FreeBSD: src/sys/modules/speaker/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/speaker

KMOD=	speaker
SRCS=	spkr.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

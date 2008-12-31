# $FreeBSD: src/sys/modules/speaker/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/speaker

KMOD=	speaker
SRCS=	spkr.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

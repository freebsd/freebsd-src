# $FreeBSD: src/sys/modules/speaker/Makefile,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/speaker

KMOD=	speaker
SRCS=	spkr.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

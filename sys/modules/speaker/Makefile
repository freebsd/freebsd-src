# $FreeBSD: src/sys/modules/speaker/Makefile,v 1.2.12.1 2005/11/16 10:50:12 ru Exp $

.PATH: ${.CURDIR}/../../dev/speaker

KMOD=	speaker
SRCS=	spkr.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

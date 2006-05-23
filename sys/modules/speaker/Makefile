# $FreeBSD: src/sys/modules/speaker/Makefile,v 1.2 2003/03/29 10:01:03 mdodd Exp $

.PATH: ${.CURDIR}/../../i386/isa

KMOD=	speaker
SRCS=	spkr.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

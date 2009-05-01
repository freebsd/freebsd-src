# $FreeBSD: src/sys/modules/io/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/io
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	io
SRCS=	iodev.c io.c
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>

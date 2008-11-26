# $FreeBSD: src/sys/modules/io/Makefile,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/io
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	io
SRCS=	iodev.c io.c
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>

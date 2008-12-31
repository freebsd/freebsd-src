# $FreeBSD: src/sys/modules/io/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/io
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	io
SRCS=	iodev.c io.c
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/io/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/io
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	io
SRCS=	iodev.c io.c
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>

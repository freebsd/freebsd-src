# $FreeBSD: src/sys/modules/io/Makefile,v 1.1 2004/08/01 11:40:53 markm Exp $

.PATH: ${.CURDIR}/../../dev/io
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	io
SRCS=	iodev.c io.c
SRCS+=	bus_if.h device_if.h vnode_if.h

.include <bsd.kmod.mk>

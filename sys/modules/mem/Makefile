# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mem
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	mem
SRCS=	memdev.c mem.c
SRCS+=	bus_if.h device_if.h vnode_if.h

.include <bsd.kmod.mk>

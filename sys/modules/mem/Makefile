# $FreeBSD: src/sys/modules/mem/Makefile,v 1.2.4.1 2004/10/21 19:32:15 markm Exp $

.PATH: ${.CURDIR}/../../dev/mem
.PATH: ${.CURDIR}/../../${MACHINE_ARCH}/${MACHINE_ARCH}

KMOD=	mem
SRCS=	memdev.c mem.c
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SRCS+=	memutil.c
.endif
.if ${MACHINE_ARCH} == "i386"
SRCS+=	i686_mem.c k6_mem.c
.endif
.if ${MACHINE_ARCH} == "amd64"
SRCS+=	amd64_mem.c
.endif
SRCS+=	bus_if.h device_if.h vnode_if.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/ndis/Makefile,v 1.11 2005/09/27 18:10:36 mlaier Exp $

.PATH: ${.CURDIR}/../../compat/ndis

KMOD=	ndis
SRCS=	subr_pe.c subr_ndis.c subr_hal.c subr_ntoskrnl.c kern_ndis.c
SRCS+=	kern_windrv.c subr_usbd.c
SRCS+=	device_if.h bus_if.h pci_if.h vnode_if.h

.if ${MACHINE_ARCH} == "amd64"
SRCS+=	winx64_wrap.S
.endif

.if ${MACHINE_ARCH} == "i386"
SRCS+=	winx32_wrap.S
.endif

.include <bsd.kmod.mk>

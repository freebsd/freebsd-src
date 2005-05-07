# $FreeBSD: src/sys/modules/ndis/Makefile,v 1.6.2.2 2005/03/31 04:24:36 wpaul Exp $

.PATH: ${.CURDIR}/../../compat/ndis

KMOD=	ndis
SRCS=	subr_pe.c subr_ndis.c subr_hal.c subr_ntoskrnl.c kern_ndis.c
SRCS+=	kern_windrv.c subr_usbd.c
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h vnode_if.h

.if ${MACHINE_ARCH} == "amd64"
SRCS+=	winx64_wrap.S
.endif

.include <bsd.kmod.mk>

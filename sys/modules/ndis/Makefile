# $FreeBSD: src/sys/modules/ndis/Makefile,v 1.6 2004/03/07 02:49:06 wpaul Exp $

.PATH: ${.CURDIR}/../../compat/ndis

KMOD=	ndis
SRCS=	subr_pe.c subr_ndis.c subr_hal.c subr_ntoskrnl.c kern_ndis.c
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h vnode_if.h

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/ndis

KMOD=	ndis
SRCS=	subr_pe.c subr_ndis.c subr_hal.c subr_ntoskrnl.c kern_ndis.c
SRCS+=	kern_windrv.c
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h vnode_if.h

.include <bsd.kmod.mk>

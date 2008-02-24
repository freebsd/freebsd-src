# $FreeBSD: src/sys/modules/drm/mga/Makefile,v 1.5 2004/01/13 11:28:48 ru Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mga
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c \
	mga_irq.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

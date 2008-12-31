# $FreeBSD: src/sys/modules/drm/mga/Makefile,v 1.5.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mga
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c \
	mga_irq.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

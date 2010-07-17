# $FreeBSD: src/sys/modules/drm/mga/Makefile,v 1.5.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mga
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c \
	mga_irq.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

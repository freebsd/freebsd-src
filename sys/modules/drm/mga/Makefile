# $FreeBSD: src/sys/modules/drm/mga/Makefile,v 1.4 2003/03/09 02:08:29 anholt Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mga
NOMAN	= YES
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c \
	mga_irq.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/drm/mga/Makefile,v 1.3 2002/05/01 01:34:48 peter Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mga
NOMAN	= YES
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

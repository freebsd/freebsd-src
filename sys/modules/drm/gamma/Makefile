# $FreeBSD: src/sys/modules/drm/gamma/Makefile,v 1.2 2002/04/28 04:58:35 anholt Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= gamma
NOMAN	= YES
SRCS    = gamma_drv.c gamma_dma.c
SRCS   += device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

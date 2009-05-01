# $FreeBSD: src/sys/modules/drm/mach64/Makefile,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mach64
SRCS	= mach64_dma.c mach64_drv.c mach64_irq.c mach64_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

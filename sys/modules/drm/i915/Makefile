# $FreeBSD: src/sys/modules/drm/i915/Makefile,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= i915
SRCS	= i915_dma.c i915_drv.c i915_irq.c i915_mem.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

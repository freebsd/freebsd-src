# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= i915
SRCS	= i915_dma.c i915_drv.c i915_irq.c i915_mem.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

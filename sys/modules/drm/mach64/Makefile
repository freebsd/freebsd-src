# $FreeBSD$

.PATH:	${SRCTOP}/sys/dev/drm
KMOD	= mach64
SRCS	= mach64_dma.c mach64_drv.c mach64_irq.c mach64_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

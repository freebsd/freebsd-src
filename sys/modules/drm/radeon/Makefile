# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
NOMAN	= YES
SRCS	= radeon_cp.c radeon_drv.c radeon_irq.c \
	radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

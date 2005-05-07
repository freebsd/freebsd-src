# $FreeBSD: src/sys/modules/drm/radeon/Makefile,v 1.5 2004/01/13 11:28:48 ru Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
SRCS	= radeon_cp.c radeon_drv.c radeon_irq.c \
	radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

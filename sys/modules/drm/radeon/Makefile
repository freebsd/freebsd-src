# $FreeBSD: src/sys/modules/drm/radeon/Makefile,v 1.6.2.1.2.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
SRCS	= r300_cmdbuf.c r600_cp.c radeon_cp.c radeon_drv.c radeon_irq.c \
	radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

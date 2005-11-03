# $FreeBSD: src/sys/modules/drm/radeon/Makefile,v 1.5.8.1 2005/08/04 17:31:01 anholt Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
SRCS	= r300_cmdbuf.c radeon_cp.c radeon_drv.c radeon_irq.c \
	radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/drm/radeon/Makefile,v 1.7.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
SRCS	= r300_cmdbuf.c r600_blit.c r600_cp.c radeon_cp.c radeon_cs.c \
	radeon_drv.c radeon_irq.c radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/drm/radeon/Makefile,v 1.4.2.1 2003/04/26 07:31:06 anholt Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
NOMAN	= YES
SRCS	= radeon_cp.c radeon_drv.c radeon_irq.c \
	radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/drm/radeon/Makefile,v 1.3 2002/05/01 01:34:48 peter Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
NOMAN	= YES
SRCS	= radeon_cp.c radeon_drv.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

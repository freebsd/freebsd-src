# $FreeBSD: src/sys/modules/drm/r128/Makefile,v 1.3 2002/05/01 01:34:48 peter Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= r128
NOMAN	= YES
SRCS	= r128_cce.c r128_drv.c r128_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

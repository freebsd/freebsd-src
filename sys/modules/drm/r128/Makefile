# $FreeBSD: src/sys/modules/drm/r128/Makefile,v 1.4 2003/03/09 02:08:29 anholt Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= r128
NOMAN	= YES
SRCS	= r128_cce.c r128_drv.c r128_irq.c r128_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

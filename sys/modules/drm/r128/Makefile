# $FreeBSD: src/sys/modules/drm/r128/Makefile,v 1.5.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= r128
SRCS	= r128_cce.c r128_drv.c r128_irq.c r128_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

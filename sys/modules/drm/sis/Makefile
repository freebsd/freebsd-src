# $FreeBSD: src/sys/modules/drm/sis/Makefile,v 1.2.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD= 	sis
SRCS=	sis_drv.c sis_ds.c sis_mm.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

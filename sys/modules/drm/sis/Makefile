# $FreeBSD: src/sys/modules/drm/sis/Makefile,v 1.1 2003/09/09 00:29:02 anholt Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD= 	sis
NOMAN=	YES
SRCS=	sis_drv.c sis_ds.c sis_mm.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

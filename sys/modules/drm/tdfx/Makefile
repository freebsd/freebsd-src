# $FreeBSD: src/sys/modules/drm/tdfx/Makefile,v 1.2 2002/04/28 04:58:40 anholt Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= tdfx
NOMAN	= YES
SRCS	= tdfx_drv.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

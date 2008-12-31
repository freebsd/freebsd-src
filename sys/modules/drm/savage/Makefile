# $FreeBSD: src/sys/modules/drm/savage/Makefile,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD= 	savage
CFLAGS += -I${.CURDIR}/../../../dev/drm
SRCS=	savage_bci.c savage_drv.c savage_state.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

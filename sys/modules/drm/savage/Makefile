# $FreeBSD: src/sys/modules/drm/savage/Makefile,v 1.1.16.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD= 	savage
CFLAGS += -I${.CURDIR}/../../../dev/drm
SRCS=	savage_bci.c savage_drv.c savage_state.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

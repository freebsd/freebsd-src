# $FreeBSD: src/sys/modules/drm/savage/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD= 	savage
CFLAGS += -I${.CURDIR}/../../../dev/drm
SRCS=	savage_bci.c savage_drv.c savage_state.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

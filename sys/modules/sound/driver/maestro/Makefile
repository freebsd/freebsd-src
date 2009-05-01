# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro.c
WERROR=

.include <bsd.kmod.mk>

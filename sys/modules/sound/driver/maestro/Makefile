# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.4.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro.c
WERROR=

.include <bsd.kmod.mk>

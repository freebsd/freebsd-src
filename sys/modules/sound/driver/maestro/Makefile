# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.4.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro.c
WERROR=

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro.c
WERROR=

.include <bsd.kmod.mk>

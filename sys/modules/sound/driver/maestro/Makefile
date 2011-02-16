# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.4.26.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro.c
WERROR=

.include <bsd.kmod.mk>

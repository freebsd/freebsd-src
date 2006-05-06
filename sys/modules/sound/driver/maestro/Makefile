# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.4 2005/01/26 16:29:07 imp Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro.c
WERROR=

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/maestro3/Makefile,v 1.3.32.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro3
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro3.c

CFLAGS+= -Wall -DM3_DEBUG_LEVEL=-1

.include <bsd.kmod.mk>

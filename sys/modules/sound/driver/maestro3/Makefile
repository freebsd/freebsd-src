# $FreeBSD: src/sys/modules/sound/driver/maestro3/Makefile,v 1.2 2001/03/24 22:33:43 peter Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro3
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	maestro3.c

CFLAGS+= -Wall -DM3_DEBUG_LEVEL=-1

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.2 2001/01/06 14:00:19 obrien Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	maestro.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/neomagic/Makefile,v 1.2 2001/01/06 14:00:19 obrien Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_neomagic
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	neomagic.c

.include <bsd.kmod.mk>

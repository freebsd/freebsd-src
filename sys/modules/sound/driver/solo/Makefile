# $FreeBSD: src/sys/modules/sound/driver/solo/Makefile,v 1.2 2001/01/06 14:00:21 obrien Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_solo
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	solo.c

.include <bsd.kmod.mk>

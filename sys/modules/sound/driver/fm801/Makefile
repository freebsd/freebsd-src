# $FreeBSD: src/sys/modules/sound/driver/fm801/Makefile,v 1.2 2001/01/06 14:00:18 obrien Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_fm801
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	fm801.c

.include <bsd.kmod.mk>

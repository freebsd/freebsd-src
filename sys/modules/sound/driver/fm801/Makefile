# $FreeBSD: src/sys/modules/sound/driver/fm801/Makefile,v 1.3.38.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_fm801
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	fm801.c

.include <bsd.kmod.mk>

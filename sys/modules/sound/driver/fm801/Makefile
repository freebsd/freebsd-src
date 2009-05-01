# $FreeBSD: src/sys/modules/sound/driver/fm801/Makefile,v 1.3.32.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_fm801
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	fm801.c

.include <bsd.kmod.mk>

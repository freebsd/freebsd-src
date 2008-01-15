# $FreeBSD: src/sys/modules/sound/driver/fm801/Makefile,v 1.3 2003/02/07 13:56:32 nyan Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_fm801
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	fm801.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/sb8/Makefile,v 1.2.2.1 2001/02/03 02:09:24 cg Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sb8
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sb8.c
KMODDEPS 	= snd_pcm snd_sbc

.include <bsd.kmod.mk>

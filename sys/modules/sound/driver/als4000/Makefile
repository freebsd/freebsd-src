# $FreeBSD: src/sys/modules/sound/driver/als4000/Makefile,v 1.1.2.2 2001/08/01 03:41:06 cg Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=		snd_als4000
SRCS=		device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=		als4000.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

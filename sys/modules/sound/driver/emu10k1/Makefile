# $FreeBSD: src/sys/modules/sound/driver/emu10k1/Makefile,v 1.1.2.2 2001/04/07 16:48:51 peter Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_emu10k1
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= emu10k1.c
KMODDEPS	= snd_pcm

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/vibes/Makefile,v 1.3.2.1 2001/04/23 22:58:26 orion Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_vibes
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= vibes.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

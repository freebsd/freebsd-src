# $FreeBSD: src/sys/modules/sound/driver/neomagic/Makefile,v 1.1.2.1 2000/09/23 19:15:05 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_neomagic
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= neomagic.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

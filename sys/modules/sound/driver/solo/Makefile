# $FreeBSD: src/sys/modules/sound/driver/solo/Makefile,v 1.1.2.1 2000/09/23 19:15:08 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_solo
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= solo.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

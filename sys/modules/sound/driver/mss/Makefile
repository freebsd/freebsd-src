# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.1.2.1 2000/09/23 19:15:05 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_mss
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= mss.c
KMODDEPS 	= snd_pcm snd_gusc

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/fm801/Makefile,v 1.1.2.1 2000/10/05 05:41:40 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_fm801
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= fm801.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

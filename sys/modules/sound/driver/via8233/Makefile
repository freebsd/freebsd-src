# $FreeBSD: src/sys/modules/sound/driver/via8233/Makefile,v 1.1.2.1 2002/08/22 17:32:49 orion Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_via8233
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= via8233.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

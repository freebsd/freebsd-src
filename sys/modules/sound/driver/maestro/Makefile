# $FreeBSD: src/sys/modules/sound/driver/maestro/Makefile,v 1.1.2.1 2000/10/05 05:44:06 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_maestro
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= maestro.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

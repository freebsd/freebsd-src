# $FreeBSD: src/sys/modules/sound/driver/ess/Makefile,v 1.1.2.2 2001/02/27 04:31:28 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_ess
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ess.c es1888.c
KMODDEPS 	= snd_pcm snd_sbc

.include <bsd.kmod.mk>

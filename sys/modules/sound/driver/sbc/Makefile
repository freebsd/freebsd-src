# $FreeBSD: src/sys/modules/sound/driver/sbc/Makefile,v 1.1.2.4 2004/07/16 07:58:53 tanimura Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_sbc
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= sbc.c
KMODDEPS	= snd_pcm

.include <bsd.kmod.mk>

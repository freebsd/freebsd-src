# $FreeBSD: src/sys/modules/sound/driver/ad1816/Makefile,v 1.1.2.2 2001/04/07 16:48:50 peter Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_ad1816
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ad1816.c
KMODDEPS	= snd_pcm

.include <bsd.kmod.mk>

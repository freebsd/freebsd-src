# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.1.2.4 2002/12/01 09:01:08 nyan Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_mss
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= mss.c gusc.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

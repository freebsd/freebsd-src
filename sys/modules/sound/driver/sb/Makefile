# $FreeBSD: src/sys/modules/sound/driver/sb/Makefile,v 1.1.2.1 2000/09/23 19:15:05 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_sb
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= sb.c
KMODDEPS 	= snd_pcm snd_sbc

.include <bsd.kmod.mk>

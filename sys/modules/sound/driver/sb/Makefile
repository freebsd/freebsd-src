# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_sb
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= sb.c
KMODDEPS 	= snd_pcm snd_sbc

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_ess
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ess.c
KMODDEPS 	= snd_pcm snd_ess1888 snd_sbc

.include <bsd.kmod.mk>

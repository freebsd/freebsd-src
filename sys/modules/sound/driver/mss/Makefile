# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_mss
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= mss.c
KMODDEPS 	= snd_pcm snd_gusc

.include <bsd.kmod.mk>

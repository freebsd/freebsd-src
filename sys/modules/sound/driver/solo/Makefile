# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_solo
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= solo.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

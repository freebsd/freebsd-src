# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_vibes
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= vibes.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

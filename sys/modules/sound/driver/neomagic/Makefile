# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_neomagic
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= neomagic.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

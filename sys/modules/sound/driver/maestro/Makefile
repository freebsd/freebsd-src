# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_maestro
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= maestro.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ich.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_cs4281
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= cs4281.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

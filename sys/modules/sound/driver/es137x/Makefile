# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_es137x
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= es137x.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

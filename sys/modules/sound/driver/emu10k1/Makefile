# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_emu10k1
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= emu10k1.c
KMODDEPS	= snd_pcm

.include <bsd.kmod.mk>

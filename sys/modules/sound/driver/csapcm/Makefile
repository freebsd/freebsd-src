# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_csapcm
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= csapcm.c
KMODDEPS 	= snd_pcm snd_csa

.include <bsd.kmod.mk>

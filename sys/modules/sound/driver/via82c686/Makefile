# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_via82c686
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= via82c686.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

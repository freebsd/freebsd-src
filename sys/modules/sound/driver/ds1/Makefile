# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ds1
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ds1.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

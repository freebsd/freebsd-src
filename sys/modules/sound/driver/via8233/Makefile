# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_via8233
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= via8233.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

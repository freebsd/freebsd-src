# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_t4dwave
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= t4dwave.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

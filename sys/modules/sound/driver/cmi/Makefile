# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_cmi
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= cmi.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

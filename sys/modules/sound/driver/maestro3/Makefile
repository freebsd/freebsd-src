# $FreeBSD$
.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_maestro3
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= maestro3.c
CFLAGS		+= -Wall

CFLAGS		+= -DM3_DEBUG_LEVEL=-1

KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/maestro3/Makefile,v 1.1.2.2 2001/03/04 08:19:02 scottl Exp $
.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_maestro3
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= maestro3.c
CFLAGS		+= -Wall

CFLAGS		+= -DM3_DEBUG_LEVEL=-1

KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

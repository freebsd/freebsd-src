# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.1.2.1 2002/08/24 08:06:13 nsayer Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/usb
KMOD		= snd_uaudio
SRCS		= device_if.h bus_if.h isa_if.h opt_usb.h
SRCS		+= uaudio.c uaudio_pcm.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

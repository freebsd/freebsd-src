# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.3.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/usb

KMOD=	snd_uaudio
SRCS=	device_if.h bus_if.h opt_usb.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>

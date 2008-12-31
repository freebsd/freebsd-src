# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/usb

KMOD=	snd_uaudio
SRCS=	device_if.h bus_if.h opt_usb.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>

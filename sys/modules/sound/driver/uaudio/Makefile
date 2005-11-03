# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.3 2004/12/29 08:50:35 imp Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/usb

KMOD=	snd_uaudio
SRCS=	device_if.h bus_if.h opt_usb.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>

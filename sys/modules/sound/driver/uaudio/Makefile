# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.2 2003/02/07 13:56:33 nyan Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/usb

KMOD=	snd_uaudio
SRCS=	device_if.h bus_if.h opt_usb.h vnode_if.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>

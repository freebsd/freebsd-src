# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.1 2002/07/21 17:28:50 nsayer Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/usb

KMOD=	snd_uaudio
SRCS=	device_if.h bus_if.h opt_usb.h vnode_if.h isa_if.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>

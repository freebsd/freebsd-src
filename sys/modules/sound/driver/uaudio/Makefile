# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.5.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

S=	${.CURDIR}/../../../..

.PATH:	$S/dev/sound/usb

KMOD=	snd_uaudio
SRCS=	bus_if.h device_if.h usb_if.h vnode_if.h
SRCS+=	opt_usb.h opt_bus.h feeder_if.h channel_if.h usbdevs.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>

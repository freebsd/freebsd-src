# $FreeBSD$

.PATH: ${.CURDIR}/../../../../dev/sound/usb

KMOD=	snd_uaudio
SRCS=	device_if.h bus_if.h opt_usb.h vnode_if.h isa_if.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>

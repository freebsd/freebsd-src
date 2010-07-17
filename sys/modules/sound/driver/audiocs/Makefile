# $FreeBSD: src/sys/modules/sound/driver/audiocs/Makefile,v 1.1.24.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/sbus

KMOD=	snd_audiocs
SRCS=	device_if.h bus_if.h ofw_bus_if.h
SRCS+=	channel_if.h feeder_if.h mixer_if.h
SRCS+=	cs4231.c

.include <bsd.kmod.mk>

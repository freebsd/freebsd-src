# $FreeBSD: src/sys/modules/sound/driver/audiocs/Makefile,v 1.1.22.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/sbus

KMOD=	snd_audiocs
SRCS=	device_if.h bus_if.h ofw_bus_if.h
SRCS+=	channel_if.h feeder_if.h mixer_if.h
SRCS+=	cs4231.c

.include <bsd.kmod.mk>

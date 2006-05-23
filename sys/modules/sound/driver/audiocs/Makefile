# $FreeBSD: src/sys/modules/sound/driver/audiocs/Makefile,v 1.1.2.1 2004/12/13 11:12:49 yongari Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/sbus

KMOD=	snd_audiocs
SRCS=	device_if.h bus_if.h ofw_bus_if.h
SRCS+=	channel_if.h feeder_if.h mixer_if.h
SRCS+=	cs4231.c

.include <bsd.kmod.mk>

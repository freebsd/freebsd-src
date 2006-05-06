# $FreeBSD: src/sys/modules/sound/driver/audiocs/Makefile,v 1.1 2004/10/25 10:29:57 yongari Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/sbus

KMOD=	snd_audiocs
SRCS=	device_if.h bus_if.h ofw_bus_if.h
SRCS+=	channel_if.h feeder_if.h mixer_if.h
SRCS+=	cs4231.c

.include <bsd.kmod.mk>

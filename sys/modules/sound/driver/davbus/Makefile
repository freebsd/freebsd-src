# $FreeBSD: src/sys/modules/sound/driver/davbus/Makefile,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/macio

KMOD=	snd_davbus
SRCS=	device_if.h bus_if.h ofw_bus_if.h
SRCS+=	channel_if.h feeder_if.h mixer_if.h
SRCS+=	aoa.c davbus.c

.include <bsd.kmod.mk>

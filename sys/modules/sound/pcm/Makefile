# $FreeBSD: src/sys/modules/sound/pcm/Makefile,v 1.12 2002/01/11 01:19:26 msmith Exp $

.PATH: ${.CURDIR}/../../../dev/sound/pcm

KMOD=	snd_pcm
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h
SRCS+=	ac97_if.c channel_if.c feeder_if.c mixer_if.c
SRCS+=	ac97.c buffer.c channel.c dsp.c fake.c
SRCS+=	feeder.c feeder_fmt.c feeder_rate.c
SRCS+=	mixer.c sndstat.c sound.c vchan.c

EXPORT_SYMS=	YES	# XXX evaluate

.include <bsd.kmod.mk>

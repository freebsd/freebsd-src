# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/sound/pcm
.PATH: ${.CURDIR}/../../../dev/sound/isa

KMOD=	snd_pcm
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h
SRCS+=	ac97_if.c channel_if.c feeder_if.c mixer_if.c
SRCS+=	ac97.c ac97_patch.c buffer.c channel.c dsp.c
SRCS+=	fake.c feeder.c feeder_fmt.c feeder_rate.c
SRCS+=	mixer.c sndstat.c sound.c vchan.c
SRCS+=	sndbuf_dma.c

EXPORT_SYMS=	YES	# XXX evaluate

.include <bsd.kmod.mk>

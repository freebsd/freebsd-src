# $FreeBSD$

.PATH:		${.CURDIR}/../../../dev/sound/pcm
KMOD		= snd_pcm
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ac97.c channel.c dsp.c fake.c
SRCS		+= feeder.c feeder_fmt.c
SRCS		+= mixer.c sound.c

.include <bsd.kmod.mk>

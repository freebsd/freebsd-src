# $FreeBSD: src/sys/modules/sound/pcm/Makefile,v 1.3.2.1 2000/09/23 19:15:09 cg Exp $

.PATH:		${.CURDIR}/../../../dev/sound/pcm
KMOD		= snd_pcm
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ac97.c channel.c dsp.c fake.c
SRCS		+= feeder.c
SRCS		+= mixer.c sound.c

.include <bsd.kmod.mk>

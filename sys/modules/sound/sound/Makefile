# $FreeBSD$

.PATH:		${.CURDIR}/../../../dev/sound/pcm
KMOD		= snd_pcm
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h opt_devfs.h
SRCS		+= ac97.c channel.c dsp.c fake.c feeder.c mixer.c sound.c

.include <bsd.kmod.mk>

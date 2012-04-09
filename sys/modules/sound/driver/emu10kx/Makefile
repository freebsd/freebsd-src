# $FreeBSD: src/sys/modules/sound/driver/emu10kx/Makefile,v 1.6.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $
.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_emu10kx

SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	isa_if.h channel_if.h ac97_if.h mixer_if.h mpufoi_if.h
SRCS+=	vnode_if.h
# Master, PCM and MIDI devices
SRCS+=	emu10kx.c
SRCS+=	emu10kx-pcm.c
SRCS+=	emu10kx-midi.c

.include <bsd.kmod.mk>

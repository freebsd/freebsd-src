# $FreeBSD$

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=		snd_als4000
SRCS=		device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=		als4000.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>

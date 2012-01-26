# $FreeBSD$

.PATH: ${.CURDIR}/../../../../dev/sound/pci \
       ${.CURDIR}/../../../../gnu/dev/sound/pci

KMOD=	snd_emu10k1
SRCS=	device_if.h bus_if.h emuxkireg.h pci_if.h
SRCS+= mpufoi_if.h
SRCS+=	emu10k1.c

.include <bsd.kmod.mk>

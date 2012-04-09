# $FreeBSD: src/sys/modules/sound/driver/emu10k1/Makefile,v 1.5.10.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_emu10k1
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+= mpufoi_if.h
SRCS+=	emu10k1.c

.include <bsd.kmod.mk>

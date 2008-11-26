# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.5.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_mss
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	mss.c gusc.c

.include <bsd.kmod.mk>

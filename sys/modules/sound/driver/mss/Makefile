# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_mss
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	mss.c gusc.c

.include <bsd.kmod.mk>

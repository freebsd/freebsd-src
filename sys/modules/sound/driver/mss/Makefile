# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.5.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_mss
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	mss.c gusc.c

.include <bsd.kmod.mk>

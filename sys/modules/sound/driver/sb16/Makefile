# $FreeBSD: src/sys/modules/sound/driver/sb16/Makefile,v 1.2.38.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sb16
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sb16.c

.include <bsd.kmod.mk>

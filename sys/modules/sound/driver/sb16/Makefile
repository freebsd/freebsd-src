# $FreeBSD: src/sys/modules/sound/driver/sb16/Makefile,v 1.2.34.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sb16
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sb16.c

.include <bsd.kmod.mk>

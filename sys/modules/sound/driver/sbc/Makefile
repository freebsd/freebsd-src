# $FreeBSD: src/sys/modules/sound/driver/sbc/Makefile,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sbc
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sbc.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/sbc/Makefile,v 1.4.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sbc
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sbc.c

.include <bsd.kmod.mk>

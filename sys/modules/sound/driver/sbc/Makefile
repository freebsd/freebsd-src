# $FreeBSD: src/sys/modules/sound/driver/sbc/Makefile,v 1.4.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sbc
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sbc.c

.include <bsd.kmod.mk>

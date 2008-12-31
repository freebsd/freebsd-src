# $FreeBSD: src/sys/modules/sound/driver/sbc/Makefile,v 1.4.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sbc
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sbc.c

.include <bsd.kmod.mk>

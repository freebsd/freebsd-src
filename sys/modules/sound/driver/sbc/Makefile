# $FreeBSD: src/sys/modules/sound/driver/sbc/Makefile,v 1.4 2002/11/06 13:46:59 nyan Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_sbc
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sbc.c

.include <bsd.kmod.mk>

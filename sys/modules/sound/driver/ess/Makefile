# $FreeBSD: src/sys/modules/sound/driver/ess/Makefile,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_ess
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	ess.c

.include <bsd.kmod.mk>

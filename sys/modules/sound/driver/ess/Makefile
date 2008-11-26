# $FreeBSD: src/sys/modules/sound/driver/ess/Makefile,v 1.3.26.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_ess
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	ess.c es1888.c

.include <bsd.kmod.mk>

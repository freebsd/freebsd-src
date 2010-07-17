# $FreeBSD: src/sys/modules/sound/driver/ess/Makefile,v 1.4.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_ess
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	ess.c

.include <bsd.kmod.mk>

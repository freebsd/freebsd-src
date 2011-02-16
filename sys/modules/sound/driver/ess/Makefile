# $FreeBSD: src/sys/modules/sound/driver/ess/Makefile,v 1.4.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_ess
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	ess.c

.include <bsd.kmod.mk>

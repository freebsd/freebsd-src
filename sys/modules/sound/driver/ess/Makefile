# $FreeBSD: src/sys/modules/sound/driver/ess/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_ess
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	ess.c

.include <bsd.kmod.mk>

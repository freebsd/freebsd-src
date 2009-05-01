# $FreeBSD: src/sys/modules/sound/driver/spicds/Makefile,v 1.2.10.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_spicds
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	spicds.c

.include <bsd.kmod.mk>

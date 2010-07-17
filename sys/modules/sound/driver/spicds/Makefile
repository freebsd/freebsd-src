# $FreeBSD: src/sys/modules/sound/driver/spicds/Makefile,v 1.2.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_spicds
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	spicds.c

.include <bsd.kmod.mk>

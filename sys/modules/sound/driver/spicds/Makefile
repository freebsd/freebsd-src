# $FreeBSD: src/sys/modules/sound/driver/spicds/Makefile,v 1.2.2.1 2007/05/13 21:03:46 ariff Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_spicds
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	spicds.c

.include <bsd.kmod.mk>

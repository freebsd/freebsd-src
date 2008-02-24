# $FreeBSD: src/sys/modules/sound/driver/spicds/Makefile,v 1.2 2006/09/30 18:12:33 netchild Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_spicds
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	spicds.c

.include <bsd.kmod.mk>

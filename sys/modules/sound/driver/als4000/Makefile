# $FreeBSD: src/sys/modules/sound/driver/als4000/Makefile,v 1.1 2001/04/23 21:53:12 orion Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_als4000
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	als4000.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/cs4281/Makefile,v 1.2 2001/03/24 22:33:43 peter Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_cs4281
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	cs4281.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/cmi/Makefile,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_cmi
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+= mpufoi_if.h
SRCS+=	cmi.c

.include <bsd.kmod.mk>

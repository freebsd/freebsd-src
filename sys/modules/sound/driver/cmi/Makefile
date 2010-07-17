# $FreeBSD: src/sys/modules/sound/driver/cmi/Makefile,v 1.4.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_cmi
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+= mpufoi_if.h
SRCS+=	cmi.c

.include <bsd.kmod.mk>

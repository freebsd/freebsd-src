# $FreeBSD: src/sys/modules/sound/driver/cmi/Makefile,v 1.4.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_cmi
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+= mpufoi_if.h
SRCS+=	cmi.c

.include <bsd.kmod.mk>

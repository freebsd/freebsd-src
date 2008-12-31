# $FreeBSD: src/sys/modules/sound/driver/cmi/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_cmi
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+= mpufoi_if.h
SRCS+=	cmi.c

.include <bsd.kmod.mk>

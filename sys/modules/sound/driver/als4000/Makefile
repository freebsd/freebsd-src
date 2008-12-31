# $FreeBSD: src/sys/modules/sound/driver/als4000/Makefile,v 1.2.30.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_als4000
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	als4000.c

.include <bsd.kmod.mk>

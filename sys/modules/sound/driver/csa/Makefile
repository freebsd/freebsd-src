# $FreeBSD: src/sys/modules/sound/driver/csa/Makefile,v 1.4.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_csa
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	csa.c csapcm.c

.include <bsd.kmod.mk>

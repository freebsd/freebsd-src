# $FreeBSD: src/sys/modules/sound/driver/csa/Makefile,v 1.4.30.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_csa
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	csa.c csapcm.c

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/sound/driver/csa/Makefile,v 1.4 2003/02/07 13:56:31 nyan Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_csa
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	csa.c csapcm.c

.include <bsd.kmod.mk>

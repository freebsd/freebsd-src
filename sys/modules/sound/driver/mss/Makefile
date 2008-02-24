# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.5 2002/11/06 13:46:59 nyan Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_mss
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	mss.c gusc.c

.include <bsd.kmod.mk>
